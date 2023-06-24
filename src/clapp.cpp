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

using rtl::application;
using namespace rtl::keyboard;

using clapp::App;

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

            if ( !g_app->setup( envir, params ) )
                return false;

            return true;
        },
        []( const application::environment& envir, const application::input& input )
        { g_app->init( envir, input ); },
        []( const application::input& input, application::output& output )
        {
            // TODO: Handle via dispatcher
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
            else if ( input.keys.pressed[keys::f9] )
            {
                g_app->toggle_stats();
            }
            else if ( input.keys.pressed[keys::f10] )
            {
                // NOTE: We fill application window with background color to prevent flickering
                // after the settings dialog appearing
                g_app->clear();

                return application::action::reset;
            }
#if RTL_ENABLE_APP_RESIZE
            else if ( input.keys.pressed[keys::f11] )
            {
                return application::action::toggle_fullscreen;
            }
#endif
            g_app->update( input, output );
            return application::action::none;
        },
        []()
        {
            g_app->shutdown();
            delete g_app;
        } );

    return 0;
}
