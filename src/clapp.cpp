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
#include <rtl/sys/application.hpp>
#include <rtl/sys/debug.hpp>

#include <clapp/app.hpp>

using rtl::Application;
using namespace rtl::keyboard;

using clapp::App;

// NOTE: Global variables help to reduce the size of the compiled code
// TODO: Implement rtl::function template to make possible to pass lambdas with capture list to RTL
// TODO: Use rtl::unique_ptr
static App*                g_app { nullptr };
static Application::Params g_app_params { 0 };

// TODO: Fix sound sluttering after window resize or move
int main( int, char*[] )
{
    // TODO: Remember fullscreen state
    // TODO: Choose fullscreen or windowed mode with predefined size via setup
    // TODO: Support for fixed size or resizable windowed modes
    Application::instance().run(
        L"CLapp",
        []( const Application::Environment& envir, Application::Params& params )
        {
            if ( !g_app )
                g_app = new App;

            if ( !g_app->setup( envir, params ) )
                return false;

            return true;
        },
        []( const Application::Environment& envir, const Application::Input& input )
        { g_app->init( envir, input ); },
        []( const Application::Input& input, Application::Output& output )
        {
            // TODO: Handle via dispatcher
            if ( input.keys.pressed[Keys::escape] )
            {
                return Application::Action::close;
            }
            else if ( input.keys.pressed[Keys::f1] )
            {
                g_app->toggle_help();
            }
            else if ( input.keys.pressed[Keys::f2] )
            {
                g_app->save_state();
            }
            else if ( input.keys.pressed[Keys::f3] )
            {
                g_app->load_state();
            }
#if CLAPP_ENABLE_ARCHITECT_MODE
            else if ( input.keys.pressed[keys::f5] )
            {
                g_app->reload_program();
            }
#endif
            else if ( input.keys.pressed[Keys::f8] && input.keys.state[Keys::control] )
            {
                g_app->reset_state();
            }
            else if ( input.keys.pressed[Keys::f9] )
            {
                g_app->toggle_stats();
            }
            else if ( input.keys.pressed[Keys::f10] )
            {
                // NOTE: We fill application window with background color to prevent flickering
                // after the settings dialog appearing
                g_app->clear();

                return Application::Action::reset;
            }
#if RTL_ENABLE_APP_RESIZE
            else if ( input.keys.pressed[Keys::f11] )
            {
                return Application::Action::toggle_fullscreen;
            }
#endif
            g_app->update( input, output );
            return Application::Action::none;
        },
        []()
        {
            g_app->shutdown();
            delete g_app;
            g_app = nullptr;
        } );

    return 0;
}
