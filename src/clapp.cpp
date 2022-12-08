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
#include <clapp/font.hpp>
#include <clapp/hud.hpp>
#include <clapp/renderer.hpp>
#include <clapp/settings.hpp>

#include <clapp.h>

using rtl::application;
using namespace rtl::keyboard;

namespace cl     = rtl::opencl;
namespace chrono = rtl::chrono;

using clapp::Context;
using clapp::Font;
using clapp::Hud;
using clapp::Renderer;
using clapp::Settings;

namespace filenames
{
    constexpr wchar_t settings[] { L"clapp.ini" };
    constexpr wchar_t save[] { L"clapp.save" };
    constexpr wchar_t auto_save[] { L"clapp.auto.save" };
    constexpr wchar_t program[] { L"clapp.cl" };
}

namespace strings
{
    // TODO: Take from resources
    constexpr wchar_t short_help_message[] { L"F1" };
    constexpr wchar_t long_help_message[] {
        L"F1 - Toggle help : F2 - Save state : F3 - Load state : F5 - Reload CL program : "
        L"F9 - Toggle stats : F10 - Show "
        L"settings dialog : F11 - Toggle fullscreen" };
}

namespace ui
{
    int font_size( int screen_width )
    {
        return 20 * screen_width / 1280;
    }
}

// TODO: move to clapp/app.hpp
class App final
{
public:
    // TODO: private
    rtl::unique_ptr<Settings> settings;
    rtl::unique_ptr<Hud>      hud;
    rtl::unique_ptr<Renderer> renderer;
    rtl::unique_ptr<Font>     font;
    rtl::unique_ptr<Context>  context;

    App()
    {
        // TODO: rtl::make_unique
        hud.reset( new Hud );
        show_help( true );
    }

    void show_help( bool show )
    {
        m_show_help = show;
        hud->set_status( m_show_help ? strings::long_help_message : strings::short_help_message );
    }

    void toggle_help() { show_help( !m_show_help ); }

    void reset_state()
    {
        context->reset_state();
        // TODO: Take from resources
        hud->add_message( L"Reset successful." );
    }

    void load_state()
    {
        if ( context->load_state( filenames::save ) )
        {
            // TODO: Take from resources
            hud->add_message( L"Loaded successfully." );
        }
    }

    void save_state()
    {
        if ( context->save_state( filenames::save ) )
        {
            // TODO: Take from resources
            hud->add_message( L"Saved successfully." );
        }
    }

    void reload_program()
    {
        context->load_program( filenames::program );
        // TODO: Take from resources
        hud->add_message( L"Program loaded successfully." );
    }

    void shutdown()
    {
        context->save_state( filenames::auto_save );
        // TODO: save/load window geometry
        settings->save( filenames::settings );
    }

private:
    bool m_show_help { true };
    bool m_pad[3] { 0 };
};

// NOTE: Global variables help to reduce the size of the compiled code
// TODO: Implement rtl::function template to make possible to pass lambdas with capture list to RTL
// TODO: Use rtl::unique_ptr
static App*                g_app { nullptr };
static application::params g_app_params { 0 };

// TODO: Fix sound sluttering after window resize or move
int main( int, char*[] )
{
    // TODO: Remember fullscreen state
    // TODO: Choose fullscreen or windowed mode with predefined size via setup
    // TODO: Support for fixed size or resizable windowed modes
    application::instance().run(
        L"CLapp",
        []( const application::environment& envir, application::params& params )
        {
            if ( !g_app )
                g_app = new App;

            if ( !g_app->settings )
            {
                // TODO: rtl::make_unique
                g_app->settings.reset( new Settings );
                g_app->settings->load( filenames::settings );

                if ( !g_app->settings->setup( nullptr, envir.display.framerate ) )
                    return false;
            }
            else
            {
                if ( !g_app->settings->setup( envir.window_handle, envir.display.framerate ) )
                    return false;

                if ( g_app->context
                     && g_app->settings->target_opencl_device().name()
                            != g_app->context->opencl_device_name() )
                {
                    // reset working context, if user selected another OpenCL device
                    g_app->context->save_state( filenames::auto_save );
                    g_app->context.reset();
                }
            }

            params.audio.samples_per_second  = g_app->settings->target_audio_sample_rate();
            params.audio.max_latency_samples = g_app->settings->target_audio_max_latency();

            return true;
        },
        []( const application::environment& envir, const application::input& input )
        {
            // NOTE: class \Context depends on OpenGL context, so we should initialize it
            // in \on_init callback which is called after OpenGL context was initialized.
            if ( !g_app->context )
            {
                // TODO: Compile source once and cache compiled binaries in the file.
                // TODO: Compile program in async manner, display status (progress???)
                // TODO: rtl::make_unique
                g_app->context.reset( new Context( g_app->settings->target_opencl_device() ) );
#if !CLAPP_ENABLE_ARCHITECT_MODE
                auto program = envir.resources.open( FILE, CLAPP_ID_OPENCL_PROGRAM );

                rtl::string_view source( static_cast<const char*>( program.data() ),
                                         program.size() );

                g_app->context->load_program( source );
#else
                g_app->context->load_program( filenames::program );
#endif
                g_app->context->load_state( filenames::auto_save );
            }

            // TODO: rtl::make_unique
            if ( !g_app->renderer )
                g_app->renderer.reset( new Renderer() );

            // TODO: rtl::make_unique
            g_app->font.reset( new Font( ui::font_size( input.screen.width ) ) );

            g_app->hud->init( input.screen.width, input.screen.height );
            g_app->renderer->init( input.screen.width, input.screen.height );
            g_app->context->init( input, g_app->renderer->texture() );
        },
        []( const application::input& input, application::output& output )
        {
            if ( input.keys.pressed[keys::escape] )
            {
                return application::action::close;
            }
            else if ( input.keys.pressed[keys::f1] )
            {
                g_app->toggle_help();
            }
            else if ( input.keys.pressed[keys::f2] )
            {
                g_app->save_state();
            }
            else if ( input.keys.pressed[keys::f3] )
            {
                g_app->load_state();
            }
#if CLAPP_ENABLE_ARCHITECT_MODE
            else if ( input.keys.pressed[keys::f5] )
            {
                g_app->reload_program();
            }
#endif
            else if ( input.keys.pressed[keys::f8] && input.keys.state[keys::control] )
            {
                g_app->reset_state();
            }
            else if ( input.keys.pressed[keys::f10] )
            {
                // NOTE: We fill application window with background color to prevent flickering
                // after the settings dialog appearing
                g_app->renderer->clear();

                return application::action::reset;
            }
#if RTL_ENABLE_APP_RESIZE
            else if ( input.keys.pressed[keys::f11] )
            {
                return application::action::toggle_fullscreen;
            }
#endif
            g_app->context->update( input, output );
            g_app->hud->update( rtl::chrono::thirds( input.clock.third_ticks ) );

            g_app->renderer->draw();
            g_app->hud->draw( *g_app->font.get() );

            return application::action::none;
        },
        []()
        {
            g_app->shutdown();
            delete g_app;
        } );

    return 0;
}
