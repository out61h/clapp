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
#pragma once

#include <rtl/memory.hpp>
#include <rtl/sys/application.hpp>

namespace clapp
{
    class Settings;
    class Hud;
    class Renderer;
    class Font;
    class Context;

    // TODO: move to clapp/app.hpp
    class App final
    {
    public:
        App();
        ~App();

        bool setup( const rtl::application::environment& envir, rtl::application::params& params );
        void init( const rtl::application::environment& envir,
                   const rtl::application::input&       input );
        void update( const rtl::application::input& input, rtl::application::output& output );
        void clear();
        void shutdown();

        void show_help( bool show );
        void toggle_help();

        void reset_state();
        void load_state();
        void save_state();

        void reload_program();

    private:
        rtl::unique_ptr<Settings> m_settings;
        rtl::unique_ptr<Hud>      m_hud;
        rtl::unique_ptr<Renderer> m_renderer;
        rtl::unique_ptr<Font>     m_font;
        rtl::unique_ptr<Context>  m_context;

        bool m_show_help { true };
        bool m_pad[3] { 0 };
    };
}
