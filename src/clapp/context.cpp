/*
 * Copyright (C) 2016-2022 Konstantin Polevik
 * All rights reserved
 *
 * This file is part of the CLapp. Redistribution and use in source and
 * binary forms, with or without modification, are permitted exclusively
 * under the terms of the MIT license. You should have received a copy of the
 * license with this file. If not, please visit:
 * https://github.com/out61h/clapp/blob/main/LICENSE.
 */
#include "context.hpp"

#include <rtl/algorithm.hpp>
#include <rtl/limits.hpp>
#include <rtl/sys/filesystem.hpp>

using namespace clapp;

namespace fs = rtl::filesystem;

Context::Context( const rtl::opencl::device& device, rtl::string_view source )
{
    context = rtl::opencl::context::create_with_current_ogl_context( device );

    program = context.build_program( source );

    kernel_input     = program.create_kernel( "main_input" );
    kernel_video_out = program.create_kernel( "main_video_out" );
    kernel_audio_out = program.create_kernel( "main_audio_out" );

    rtl::vector<rtl::uint32_t> state( state_buffer_size, 0 );

    for ( auto& buffer : buffer_state )
        buffer = context.create_buffer_1d_uint( state.size(), state.data() );

    buffer_keys = context.create_buffer_1d_uint( keys_count );
}

void Context::init( [[maybe_unused]] const rtl::application::input& input, unsigned gl_texture )
{
    audio_data_left.resize( input.audio.samples_per_frame );
    audio_data_right.resize( input.audio.samples_per_frame );

    buffer_audio_left  = context.create_buffer_1d_float( input.audio.samples_per_frame );
    buffer_audio_right = context.create_buffer_1d_float( input.audio.samples_per_frame );

    buffer_video = context.create_buffer_2d_from_ogl_texture( gl_texture );
}

void Context::update( [[maybe_unused]] const rtl::application::input& input,
                      [[maybe_unused]] rtl::application::output&      output )
{
    context.enqueue_copy( input.keys.state, buffer_keys, buffer_keys.length() );

    // TODO: remove; do copying of unchanged cells inside kernels
    context.enqueue_copy( buffer_state[1u - buffer_state_output_index],
                          buffer_state[buffer_state_output_index] );

    kernel_input.args()
        .arg( buffer_state[1u - buffer_state_output_index] ) // current state
        .arg( buffer_state[buffer_state_output_index] ) // next state

        .arg( audio_samples_generated )
        .arg( input.audio.samples_per_frame )
        .arg( input.audio.samples_per_second )

        .arg( input.screen.width )
        .arg( input.screen.height )

        // TODO: mouse cursor relative coords
        .arg( 0.5f )
        .arg( 0.5f )

        .arg( buffer_keys )
        .arg( buffer_keys.length() );

    context.enqueue_process_1d( kernel_input, buffer_state[buffer_state_output_index].length() );
    context.wait();

    kernel_video_out.args()
        .arg( buffer_state[buffer_state_output_index] )
        .arg( buffer_state[buffer_state_output_index].length() )
        .arg( buffer_video );

    context.enqueue_acquire_ogl_object( buffer_video );
    context.enqueue_process_2d( kernel_video_out, (size_t)input.screen.width,
                                (size_t)input.screen.height );
    context.enqueue_release_ogl_object( buffer_video );

    kernel_audio_out.args()
        .arg( buffer_state[buffer_state_output_index] )
        .arg( buffer_state[buffer_state_output_index].length() )
        .arg( buffer_audio_left )
        .arg( buffer_audio_right )
        .arg( input.audio.samples_per_frame )
        .arg( input.audio.samples_per_second );

    context.enqueue_process_1d( kernel_audio_out, input.audio.samples_per_frame );
    context.enqueue_copy( buffer_audio_left, audio_data_left.data(),
                          input.audio.samples_per_frame );
    context.enqueue_copy( buffer_audio_right, audio_data_right.data(),
                          input.audio.samples_per_frame );

    buffer_state_output_index = 1u - buffer_state_output_index;

    context.wait();

    // TODO: convert sample format inside audio kernel
    constexpr float max_int16 = (float)rtl::numeric_limits<rtl::int16_t>::max();

    rtl::int16_t* samples = input.audio.frame;

    for ( size_t i = 0; i < input.audio.samples_per_frame; ++i )
    {
        *samples++
            = (rtl::int16_t)rtl::clamp( audio_data_left[i] * max_int16, -max_int16, max_int16 );
        *samples++
            = (rtl::int16_t)rtl::clamp( audio_data_right[i] * max_int16, -max_int16, max_int16 );
    }

    audio_samples_generated += input.audio.samples_per_frame;
}

bool Context::save( const wchar_t* filename )
{
    rtl::opencl::buffer& buffer = buffer_state[1 - buffer_state_output_index];

    rtl::vector<rtl::uint32_t> state( buffer.length(), 0 );
    context.enqueue_copy( buffer, state.data(), buffer.length() );
    context.wait();

    // TODO: save to tmp file, than rename
    auto f
        = fs::file::open( filename, fs::file::access::write_only, fs::file::mode::create_always );
    if ( !f )
        return false;

    const unsigned bytes_to_write = state.size() * sizeof( rtl::uint32_t );

    return f.write( state.data(), bytes_to_write ) == bytes_to_write;
}

bool Context::load( const wchar_t* filename )
{
    auto f = fs::file::open( filename, fs::file::access::read_only, fs::file::mode::open_existing );
    if ( !f )
        return false;

    f.seek( 0, fs::file::position::end );

    const size_t cells_count = static_cast<size_t>( f.tell() ) / sizeof( rtl::uint32_t );

    // TODO: check using version number and notify user if mismatch detected
    if ( cells_count != state_buffer_size )
        return false;

    rtl::vector<rtl::uint32_t> state( cells_count );

    f.seek( 0, fs::file::position::begin );

    const unsigned bytes_to_read = cells_count * sizeof( rtl::uint32_t );

    if ( f.read( state.data(), bytes_to_read ) != bytes_to_read )
        return false;

    rtl::opencl::buffer& buffer = buffer_state[1 - buffer_state_output_index];

    context.enqueue_copy( state.data(), buffer, buffer.length() );
    context.wait();

    return true;
}
