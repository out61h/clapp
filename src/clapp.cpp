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
#include <rtl/sys/filesystem.hpp>
#include <rtl/sys/opencl.hpp>

#include <clapp/context.hpp>
#include <clapp/renderer.hpp>

#if !CLAPP_ENABLE_ARCHITECT_MODE
#include "p.h"
#endif

using rtl::application;
using namespace rtl::keyboard;

namespace cl = rtl::opencl;

using clapp::Context;
using clapp::Renderer;

#include <cl/opencl.h>

// NOTE: Global variables is used for reducing the size of compiled code
static Context*            g_context { nullptr };
static Renderer*           g_renderer { nullptr };
static application::params g_app_params { 0 };
static constexpr wchar_t   g_save_filename[] { L"clapp.save" };
static constexpr wchar_t   g_auto_save_filename[] { L"clapp.auto.save" };

int main( int, char*[] )
{
    // TODO: Fix sound sluttering after window resize or move
    // TODO: Add settings dialog to tune sound params and select OpenCL device
    // TODO: Remember last settings
    g_app_params.audio.samples_per_second  = 48000;
    g_app_params.audio.max_latency_samples = 16000;

    application::instance().run(
        L"clapp", g_app_params,
        // TODO: implement rtl::function template
        []( const application::input& input )
        {
            if ( !g_context )
            {
                // TODO: Do selection via settings dialog.
                // TODO: Add Option to filter devices supported cl_khr_gl_sharing
                // auto platforms = cl::platform::query_list_by_extension( "cl_khr_gl_sharing" );
                auto platforms = cl::platform::query_list();
                auto devices   = cl::device::query_list( platforms );

            // TODO: do we really need external cdata loading?
#if !CLAPP_ENABLE_ARCHITECT_MODE
                rtl::string_view source( (const char*)program_i, (size_t)program_i_size );
#else
            // TODO: load source from file
#endif
                // TODO: Compile source once and store compiled binaries in file.
                g_context = new Context( devices.front(), source );
                g_context->load( g_auto_save_filename );
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
            else if ( input.keys.pressed[keys::f11] )
            {
                return application::action::toggle_fullscreen;
            }
            else if ( input.keys.pressed[keys::f2] )
            {
                if ( g_context->save( g_save_filename ) )
                {
                    // TODO: Display "SAVE WAS SUCCESSFULL" OSD message
                }
            }
            else if ( input.keys.pressed[keys::f3] )
            {
                if ( g_context->load( g_save_filename ) )
                {
                    // TODO: Display "LOAD WAS SUCCESSFULL" OSD message
                }
            }
#if CLAPP_ENABLE_ARCHITECT_MODE
            else if ( input.keys.pressed[keys::f5] )
            {
                // TODO: reload program from external file
                // TODO: Display "RELOAD WAS SUCCESSFULL" OSD message
            }
#endif
            // TODO: Ctrl+F8 = Reset state
            // TODO: F10 = Show setup dialog

            g_context->update( input, output );
            g_renderer->draw();

            return application::action::none;
        },
        []()
        {
            g_context->save( g_auto_save_filename );
            delete g_context;
            delete g_renderer;
        } );

    return 0;
}
