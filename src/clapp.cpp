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
#include <rtl/memory.hpp>
#include <rtl/string.hpp>

#include <rtl/sys/application.hpp>
#include <rtl/sys/debug.hpp>
#include <rtl/sys/opencl.hpp>

#include <clapp/context.hpp>
#include <clapp/renderer.hpp>
#include <clapp/settings.hpp>

#include <clapp.h>

using rtl::application;
using namespace rtl::keyboard;

namespace cl = rtl::opencl;

using clapp::Context;
using clapp::Renderer;
using clapp::Settings;

// NOTE: Global variables help to reduce the size of the compiled code
// TODO: implement rtl::function template to make possible to pass lambdas with capture list to RTL
static Context*            g_context { nullptr };
static Renderer*           g_renderer { nullptr };
static Settings*           g_settings { nullptr };
static application::params g_app_params { 0 };
static constexpr wchar_t   g_settings_filename[] { L"clapp.ini" };
static constexpr wchar_t   g_save_filename[] { L"clapp.save" };
static constexpr wchar_t   g_auto_save_filename[] { L"clapp.auto.save" };
static constexpr wchar_t   g_program_filename[] { L"clapp.cl" };

int main( int, char*[] )
{
    // TODO: Choose fullscreen or windowed mode with predefined size via setup
    // TODO: Support for fixed size or resizable windowed modes
    // TODO: Framerate downscaler 1:1, 1:2, 1:3, 1:4, 1:5, 1:6, 1:8
    // TODO: Resolution downscaler 1:1, 1:2, 1:4, 1:8
    // TODO: Add OSD keyboard hints or "F1 - help" to the main window title
    // TODO: Fix sound sluttering after window resize or move
    // TODO: Update window content while moving
    application::instance().run(
        L"CLapp",
        []( const application::environment& envir, application::params& params )
        {
            if ( !g_settings )
            {
                g_settings = new Settings;
                g_settings->load( g_settings_filename );

                if ( !g_settings->setup( nullptr, envir.display.framerate ) )
                    return false;
            }
            else
            {
                if ( !g_settings->setup( envir.window_handle, envir.display.framerate ) )
                    return false;

                if ( g_context
                     && g_settings->target_opencl_device().name()
                         != g_context->opencl_device_name() )
                {
                    // NOTE: Resets working context, if user selected another OpenCL device
                    g_context->save_state( g_auto_save_filename );
                    delete g_context;
                    g_context = nullptr;
                }
            }

            params.audio.samples_per_second  = g_settings->target_audio_sample_rate();
            params.audio.max_latency_samples = g_settings->target_audio_max_latency();

            return true;
        },
        []( const application::environment& envir, const application::input& input )
        {
            // NOTE: class \Context depends on OpenGL context, so we should initialize it
            // in \on_init callback which is called after OpenGL context was initialized.
            if ( !g_context )
            {
                // TODO: Compile source once and cache compiled binaries in the file.
                // TODO: Async program compiling
                g_context = new Context( g_settings->target_opencl_device() );
#if !CLAPP_ENABLE_ARCHITECT_MODE
                auto program = envir.resources.open( FILE, CLAPP_ID_OPENCL_PROGRAM );

                rtl::string_view source( static_cast<const char*>( program.data() ),
                                         program.size() );

                g_context->load_program( source );
#else
                g_context->load_program( g_program_filename );
#endif
                g_context->load_state( g_auto_save_filename );
            }

            if ( !g_renderer )
                g_renderer = new Renderer();

            g_renderer->init( input.screen.width, input.screen.height );
            g_context->init( input, g_renderer->texture() );
        },
        []( const application::input& input, application::output& output )
        {
            if ( input.keys.pressed[keys::escape] )
            {
                return application::action::close;
            }
            else if ( input.keys.pressed[keys::f2] )
            {
                if ( g_context->save_state( g_save_filename ) )
                {
                    // TODO: Display "SAVE WAS SUCCESSFULL" OSD message
                }
            }
            else if ( input.keys.pressed[keys::f3] )
            {
                if ( g_context->load_state( g_save_filename ) )
                {
                    // TODO: Display "LOAD WAS SUCCESSFULL" OSD message
                }
            }
#if CLAPP_ENABLE_ARCHITECT_MODE
            else if ( input.keys.pressed[keys::f5] )
            {
                g_context->load_program( g_program_filename );
                // TODO: Display "RELOAD WAS SUCCESSFULL" OSD message
            }
#endif
            else if ( input.keys.pressed[keys::f8] && input.keys.state[keys::control] )
            {
                g_context->reset_state();
                // TODO: Display "STATE RESET" OSD message
            }
            else if ( input.keys.pressed[keys::f10] )
            {
                // NOTE: We fill the window with background color to prevent flickering after the
                // settings dialog appearing
                g_renderer->clear();

                return application::action::reset;
            }
#if RTL_ENABLE_APP_RESIZE
            else if ( input.keys.pressed[keys::f11] )
            {
                return application::action::toggle_fullscreen;
            }
#endif
            g_context->update( input, output );
            g_renderer->draw();

            return application::action::none;
        },
        []()
        {
            g_context->save_state( g_auto_save_filename );
            delete g_context;
            delete g_renderer;

            // TODO: save/load window geometry
            g_settings->save( g_settings_filename );
            delete g_settings;
        } );

    return 0;
}
