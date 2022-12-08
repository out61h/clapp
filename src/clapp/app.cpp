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
#include "app.hpp"
#include "context.hpp"
#include "font.hpp"
#include "hud.hpp"
#include "renderer.hpp"
#include "settings.hpp"

#include <clapp.h>

using namespace clapp;

namespace strings
{
    // TODO: Take from resources
    constexpr wchar_t short_help_message[] { L"F1" };
    constexpr wchar_t long_help_message[] {
        L"F1 - Toggle help : F2 - Save state : F3 - Load state : F5 - Reload CL program : "
        L"F9 - Toggle stats : F10 - Show "
        L"settings dialog : F11 - Toggle fullscreen" };
}

namespace filenames
{
    constexpr wchar_t settings[] { L"clapp.ini" };
    constexpr wchar_t save[] { L"clapp.save" };
    constexpr wchar_t auto_save[] { L"clapp.auto.save" };
    constexpr wchar_t program[] { L"clapp.cl" };
}

namespace ui
{
    int font_size( int screen_width )
    {
        return 20 * screen_width / 1280;
    }
}

App::App()
{
    // TODO: rtl::make_unique
    hud.reset( new Hud );
    show_help( true );
}

App::~App() = default;

void App::show_help( bool show )
{
    m_show_help = show;
    hud->set_status( m_show_help ? strings::long_help_message : strings::short_help_message );
}

void App::toggle_help()
{
    show_help( !m_show_help );
}

void App::reset_state()
{
    context->reset_state();
    // TODO: Take from resources
    hud->add_message( L"Reset successful." );
}

void App::load_state()
{
    if ( context->load_state( filenames::save ) )
    {
        // TODO: Take from resources
        hud->add_message( L"Loaded successfully." );
    }
}

void App::save_state()
{
    if ( context->save_state( filenames::save ) )
    {
        // TODO: Take from resources
        hud->add_message( L"Saved successfully." );
    }
}

void App::reload_program()
{
    context->load_program( filenames::program );
    // TODO: Take from resources
    hud->add_message( L"Program loaded successfully." );
}

bool App::setup( const rtl::application::environment& envir, rtl::application::params& params )
{
    if ( !settings )
    {
        // TODO: rtl::make_unique
        settings.reset( new Settings );
        settings->load( filenames::settings );

        if ( !settings->setup( nullptr, envir.display.framerate ) )
            return false;
    }
    else
    {
        if ( !settings->setup( envir.window_handle, envir.display.framerate ) )
            return false;

        if ( context && settings->target_opencl_device().name() != context->opencl_device_name() )
        {
            // reset working context, if user selected another OpenCL device
            context->save_state( filenames::auto_save );
            context.reset();
        }
    }

    params.audio.samples_per_second  = settings->target_audio_sample_rate();
    params.audio.max_latency_samples = settings->target_audio_max_latency();

    return true;
}

void App::init( const rtl::application::environment& envir, const rtl::application::input& input )
{
    // NOTE: class \Context depends on OpenGL context, so we should initialize it
    // in \on_init callback which is called after OpenGL context was initialized.
    if ( !context )
    {
        // TODO: Compile source once and cache compiled binaries in the file.
        // TODO: Compile program in async manner, display status (progress???)
        // TODO: rtl::make_unique
        context.reset( new Context( settings->target_opencl_device() ) );
#if !CLAPP_ENABLE_ARCHITECT_MODE
        auto program = envir.resources.open( FILE, CLAPP_ID_OPENCL_PROGRAM );

        rtl::string_view source( static_cast<const char*>( program.data() ), program.size() );

        context->load_program( source );
#else
        context->load_program( filenames::program );
#endif
        context->load_state( filenames::auto_save );
    }

    // TODO: rtl::make_unique
    if ( !renderer )
        renderer.reset( new Renderer() );

    // TODO: rtl::make_unique
    font.reset( new Font( ui::font_size( input.screen.width ) ) );

    hud->init( input.screen.width, input.screen.height );
    renderer->init( input.screen.width, input.screen.height );
    context->init( input, renderer->texture() );
}

void App::update( const rtl::application::input& input, rtl::application::output& output )
{
    context->update( input, output );
    hud->update( rtl::chrono::thirds( input.clock.third_ticks ) );

    renderer->draw();
    hud->draw( *font.get() );
}

void App::clear()
{
    renderer->clear();
}

void App::shutdown()
{
    context->save_state( filenames::auto_save );
    // TODO: save/load window geometry
    settings->save( filenames::settings );
}
