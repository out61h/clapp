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

Context::Context( const rtl::opencl::device& device )
    : m_device_name( device.name() )
{
    // cppcheck-suppress useInitializationList
    m_context = rtl::opencl::context::create_with_current_ogl_context( device );

    rtl::vector<rtl::uint32_t> state( state_buffer_size, 0 );

    for ( auto& buffer : m_buffer_state )
        buffer = m_context.create_buffer_1d_uint( state.size(), state.data() );

    m_buffer_keys = m_context.create_buffer_1d_uint( keys_count );
}

void Context::load_program( const wchar_t* filename )
{
    using rtl::filesystem::file;

    file f = file::open( filename, file::access::read_only, file::mode::open_existing );
    f.seek( 0, file::position::end );
    const size_t f_size = static_cast<size_t>( f.tell() );
    f.seek( 0, file::position::begin );

    rtl::string source( f_size, 0 );
    f.read( source.data(), f_size );

    load_program( source );
}

void Context::load_program( rtl::string_view source )
{
    m_program = m_context.build_program( source );

    m_kernel_input     = m_program.create_kernel( "main_input" );
    m_kernel_video_out = m_program.create_kernel( "main_video_out" );
    m_kernel_audio_out = m_program.create_kernel( "main_audio_out" );
}

void Context::init( [[maybe_unused]] const rtl::application::input& input, unsigned gl_texture )
{
    m_audio_data_left.resize( input.audio.samples_per_frame );
    m_audio_data_right.resize( input.audio.samples_per_frame );

    m_buffer_audio_left  = m_context.create_buffer_1d_float( input.audio.samples_per_frame );
    m_buffer_audio_right = m_context.create_buffer_1d_float( input.audio.samples_per_frame );

    m_buffer_video = m_context.create_buffer_2d_from_ogl_texture( gl_texture );
}

void Context::update( [[maybe_unused]] const rtl::application::input& input,
                      [[maybe_unused]] rtl::application::output&      output )
{
    m_context.enqueue_copy( input.keys.state, m_buffer_keys, m_buffer_keys.length() );

    // TODO: remove; do copying of unchanged cells inside kernels
    m_context.enqueue_copy( m_buffer_state[1u - m_buffer_state_output_index],
                            m_buffer_state[m_buffer_state_output_index] );

    m_kernel_input.args()
        .arg( m_buffer_state[1u - m_buffer_state_output_index] ) // current state
        .arg( m_buffer_state[m_buffer_state_output_index] ) // next state

        .arg( m_audio_samples_generated )
        .arg( input.audio.samples_per_frame )
        .arg( input.audio.samples_per_second )

        .arg( input.screen.width )
        .arg( input.screen.height )

        // TODO: mouse cursor relative coords
        .arg( 0.5f )
        .arg( 0.5f )

        .arg( m_buffer_keys )
        .arg( m_buffer_keys.length() );

    m_context.enqueue_process_1d( m_kernel_input,
                                  m_buffer_state[m_buffer_state_output_index].length() );
    m_context.wait();

    m_kernel_video_out.args()
        .arg( m_buffer_state[m_buffer_state_output_index] )
        .arg( m_buffer_state[m_buffer_state_output_index].length() )
        .arg( m_buffer_video );

    m_context.enqueue_acquire_ogl_object( m_buffer_video );
    m_context.enqueue_process_2d( m_kernel_video_out,
                                  static_cast<size_t>( input.screen.width ),
                                  static_cast<size_t>( input.screen.height ) );
    m_context.enqueue_release_ogl_object( m_buffer_video );

    m_kernel_audio_out.args()
        .arg( m_buffer_state[m_buffer_state_output_index] )
        .arg( m_buffer_state[m_buffer_state_output_index].length() )
        .arg( m_buffer_audio_left )
        .arg( m_buffer_audio_right )
        .arg( input.audio.samples_per_frame )
        .arg( input.audio.samples_per_second );

    m_context.enqueue_process_1d( m_kernel_audio_out, input.audio.samples_per_frame );
    m_context.enqueue_copy( m_buffer_audio_left,
                            m_audio_data_left.data(),
                            input.audio.samples_per_frame );
    m_context.enqueue_copy( m_buffer_audio_right,
                            m_audio_data_right.data(),
                            input.audio.samples_per_frame );

    m_buffer_state_output_index = 1u - m_buffer_state_output_index;

    m_context.wait();

    // TODO: convert sample format inside audio kernel
    constexpr float max_int16 = (float)rtl::numeric_limits<rtl::int16_t>::max();

    rtl::int16_t* samples = input.audio.frame;

    for ( size_t i = 0; i < input.audio.samples_per_frame; ++i )
    {
        *samples++
            = (rtl::int16_t)rtl::clamp( m_audio_data_left[i] * max_int16, -max_int16, max_int16 );
        *samples++
            = (rtl::int16_t)rtl::clamp( m_audio_data_right[i] * max_int16, -max_int16, max_int16 );
    }

    m_audio_samples_generated += input.audio.samples_per_frame;
}

bool Context::save_state( const wchar_t* filename )
{
    rtl::opencl::buffer& buffer = m_buffer_state[1 - m_buffer_state_output_index];

    rtl::vector<rtl::uint32_t> state( buffer.length(), 0 );
    m_context.enqueue_copy( buffer, state.data(), buffer.length() );
    m_context.wait();

    // TODO: save to tmp file, than rename
    auto f
        = fs::file::open( filename, fs::file::access::write_only, fs::file::mode::create_always );
    if ( !f )
        return false;

    const unsigned bytes_to_write = state.size() * sizeof( rtl::uint32_t );

    return f.write( state.data(), bytes_to_write ) == bytes_to_write;
}

bool Context::load_state( const wchar_t* filename )
{
    auto f = fs::file::open( filename, fs::file::access::read_only, fs::file::mode::open_existing );
    if ( !f )
        return false;

    f.seek( 0, fs::file::position::end );

    const size_t cells_count = static_cast<size_t>( f.tell() ) / sizeof( rtl::uint32_t );

    // TODO: check using version number and notify user if mismatch detected
    if ( cells_count != state_buffer_size )
        return false;

    rtl::vector<rtl::uint32_t> state( cells_count, 0 );

    f.seek( 0, fs::file::position::begin );

    const unsigned bytes_to_read = cells_count * sizeof( rtl::uint32_t );

    if ( f.read( state.data(), bytes_to_read ) != bytes_to_read )
        return false;

    rtl::opencl::buffer& buffer = m_buffer_state[1 - m_buffer_state_output_index];

    m_context.enqueue_copy( state.data(), buffer, buffer.length() );
    m_context.wait();

    return true;
}

void Context::reset_state()
{
    rtl::vector<rtl::uint32_t> state( state_buffer_size, 0 );

    rtl::opencl::buffer& buffer = m_buffer_state[1 - m_buffer_state_output_index];

    m_context.enqueue_copy( state.data(), buffer, buffer.length() );
    m_context.wait();
}
