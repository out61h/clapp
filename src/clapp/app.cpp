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
    : m_hud( rtl::make_unique<Hud>() )
{
    show_help( true );
}

App::~App() = default;

void App::show_help( bool show )
{
    m_show_help = show;
    m_hud->set_status( m_show_help ? strings::long_help_message : strings::short_help_message );
}

void App::toggle_help()
{
    show_help( !m_show_help );
}

void App::show_stats( bool show )
{
    m_show_stats = show;

    if ( !m_show_stats )
    {
        m_hud->set_stat_line( 0, L"" );
        m_hud->set_stat_line( 1, L"" );
    }
}

void App::toggle_stats()
{
    show_stats( !m_show_stats );
}

void App::reset_state()
{
    m_context->reset_state();
    // TODO: Take from resources
    m_hud->add_message( L"Reset successful." );
}

void App::load_state()
{
    if ( m_context->load_state( filenames::save ) )
    {
        // TODO: Take from resources
        m_hud->add_message( L"Loaded successfully." );
    }
}

void App::save_state()
{
    if ( m_context->save_state( filenames::save ) )
    {
        // TODO: Take from resources
        m_hud->add_message( L"Saved successfully." );
    }
}

void App::reload_program()
{
    m_context->load_program( filenames::program );
    // TODO: Take from resources
    m_hud->add_message( L"Program loaded successfully." );
}

bool App::setup( const rtl::Application::Environment& envir, rtl::Application::Params& params )
{
    if ( !m_settings )
    {
        m_settings = rtl::make_unique<Settings>();
        m_settings->load( filenames::settings );

        if ( !m_settings->setup( nullptr, envir.display.framerate ) )
            return false;
    }
    else
    {
        if ( !m_settings->setup( envir.window_handle, envir.display.framerate ) )
            return false;

        if ( m_context
             && m_settings->target_opencl_device().name() != m_context->opencl_device_name() )
        {
            // reset working context, if user selected another OpenCL device
            m_context->save_state( filenames::auto_save );
            m_context.reset();
        }
    }

    params.audio.samples_per_second  = m_settings->target_audio_sample_rate();
    params.audio.max_latency_samples = m_settings->target_audio_max_latency();

    return true;
}

void App::init( const rtl::Application::Environment& envir, const rtl::Application::Input& input )
{
    // NOTE: class \Context depends on OpenGL context, so we should initialize it
    // in \on_init callback which is called after OpenGL context was initialized.
    if ( !m_context )
    {
        // TODO: Compile source once and cache compiled binaries in the file.
        // TODO: Compile program in async manner, display status (progress???)
        m_context = rtl::make_unique<Context>( m_settings->target_opencl_device() );
#if !CLAPP_ENABLE_ARCHITECT_MODE
        auto program = envir.resources.open( FILE, CLAPP_ID_OPENCL_PROGRAM );

        rtl::string_view source( static_cast<const char*>( program.data() ), program.size() );

        m_context->load_program( source );
#else
        context->load_program( filenames::program );
#endif
        m_context->load_state( filenames::auto_save );
    }

    if ( !m_renderer )
        m_renderer = rtl::make_unique<Renderer>();

    m_font = rtl::make_unique<Font>( ui::font_size( input.screen.width ) );

    m_hud->init( input.screen.width, input.screen.height );
    m_renderer->init( input.screen.width, input.screen.height );
    m_context->init( input, m_renderer->texture() );
}

void App::update( const rtl::Application::Input& input, rtl::Application::Output& output )
{
    auto start = rtl::chrono::steady_clock::now();

    rtl::chrono::microseconds ft = start - m_frame_start;

    m_frame_start = rtl::chrono::steady_clock::now();

    m_context->update( input, output );
    m_hud->update( rtl::chrono::thirds( input.clock.third_ticks ) );

    m_renderer->draw();
    m_hud->draw( *m_font.get() );

    auto end = rtl::chrono::steady_clock::now();

    rtl::chrono::microseconds delta = end - start;
    // process delta in ms

    if ( m_show_stats )
    {
        m_hud->set_stat_line( 0,
                              rtl::wstring( L"Render time:  " )
                                  + rtl::to_wstring( delta.count() ) );
        m_hud->set_stat_line( 1, rtl::wstring( L"Frame time:  " ) + rtl::to_wstring( ft.count() ) );

        m_hud->set_stat_line( 2, rtl::wstring( L"Audio underruns: " ) );
        m_hud->set_stat_line( 3, rtl::wstring( L"Audio overruns: " ) );
        m_hud->set_stat_line( 4, rtl::wstring( L"Audio latency: " ) );
    }
}

void App::clear()
{
    m_renderer->clear();
}

void App::shutdown()
{
    m_context->save_state( filenames::auto_save );
    // TODO: save/load window geometry
    m_settings->save( filenames::settings );
}
