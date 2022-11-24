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

#pragma warning( push )
#pragma warning( disable : 4668 )
#include <Windows.h>
#include <gl/GL.h>
#pragma warning( pop )

#include <clapp/context.hpp>

// TODO: Add switch for using embed code or reading it from file
#include "p.h"

using rtl::application;
using namespace rtl::keyboard;

namespace cl = rtl::opencl;

using clapp::Context;

#include <cl/opencl.h>

// NOTE: Global variables is used for reducing the size of compiled code
static Context*                g_context { nullptr };
static application::params     g_app_params { 0 };
static rtl::uint32_t           g_cdata[256] { 0 };
static GLuint                  g_texture { 0 };
static constexpr const wchar_t g_save_filename[] { L"clapp.save" };
static constexpr const wchar_t g_auto_save_filename[] { L"clapp.auto.save" };

int main( int, char*[] )
{
    // TODO: VSYNC every 2nd, 3rd, 4th... frame
    // TODO: Fix sound sluttering after window resize or move
    // TODO: Add settings dialog to tune sound params and select OpenCL device
    // TODO: Remember last settings
    g_app_params.audio.samples_per_second  = 48000;
    g_app_params.audio.max_latency_samples = 16000;

    // TODO: add fps meter (with quantils)
    // TODO: add sound buffer overrund/underruns counters

    // TODO: RTL_OPENGL_CHECK macro?
    application::instance().run(
        L"clapp", g_app_params,
        // TODO: implement rtl::function template
        []( const application::input& input )
        {
            if ( !g_context )
            {
                // TODO: select
                // auto platforms = cl::platform::query_list_by_extension( "cl_khr_gl_sharing" );
                auto platforms = cl::platform::query_list();
                auto devices   = cl::device::query_list( platforms );

                // TODO: do we really need external cdata loading?
                g_context = new Context(
                    devices.front(),
                    rtl::string_view( (const char*)program_i, (size_t)program_i_size ), g_cdata,
                    256 );

                g_context->load( g_auto_save_filename );
            }

            // TODO: extract to renderer component
            ::glViewport( 0, 0, input.screen.width, input.screen.height );
            ::glDisable( GL_LIGHTING );
            ::glEnable( GL_TEXTURE_2D );
            ::glClearColor( 1.0f, 1.0f, 1.0f, 1.0f );

            if ( ::glIsTexture( g_texture ) )
                ::glDeleteTextures( 1, &g_texture );

            ::glGenTextures( 1, &g_texture );
            ::glBindTexture( GL_TEXTURE_2D, g_texture );
            ::glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)input.screen.width,
                            (GLsizei)input.screen.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );

            g_context->init( input, g_texture );
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
                    // TODO: Display "SAVE SUCCESS" message
                }
            }
            else if ( input.keys.pressed[keys::f3] )
            {
                if ( g_context->load( g_save_filename ) )
                {
                    // TODO: Display "LOAD SUCCESS" message
                }
            }
            else if ( input.keys.pressed[keys::f5] )
            {
                // TODO: reload program from external file
            }

            g_context->update( input, output );

            // TODO: extract to renderer component
            // TODO: Use OpenGL 3.x API with shaders
            ::glClear( GL_COLOR_BUFFER_BIT );

            ::glMatrixMode( GL_PROJECTION );
            ::glLoadIdentity();

            ::glBindTexture( GL_TEXTURE_2D, g_texture );
            ::glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
            ::glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
            ::glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
            ::glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

            ::glOrtho( 0.0, input.screen.width, 0.0, input.screen.height, -1.0, 1.0 );

            ::glMatrixMode( GL_MODELVIEW );
            ::glLoadIdentity();
            ::glBegin( GL_QUADS );
            ::glColor3f( 1.0f, 1.0f, 1.0f );
            ::glTexCoord2f( 0.f, 0.f );
            ::glVertex2i( 0, input.screen.height );
            ::glTexCoord2f( 1.f, 0.f );
            ::glVertex2i( input.screen.width, input.screen.height );
            ::glTexCoord2f( 1.f, 1.f );
            ::glVertex2i( input.screen.width, 0 );
            ::glTexCoord2f( 0.f, 1.f );
            ::glVertex2i( 0, 0 );
            ::glEnd();

            ::glFlush();

            return application::action::none;
        },
        []()
        {
            g_context->save( g_auto_save_filename );
            delete g_context;
        } );

    return 0;
}
