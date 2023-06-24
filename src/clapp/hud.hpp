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

#include <rtl/array.hpp>
#include <rtl/chrono.hpp>
#include <rtl/string.hpp>

namespace clapp
{
    class Renderer;
    class Font;

    class Hud final
    {
    public:
        void init( int screen_width, int screen_height );

        void set_status( rtl::wstring_view text );
        void add_message( rtl::wstring_view text );

        void set_stat_line( unsigned index, rtl::wstring_view text );

        void update( rtl::chrono::thirds clock );
        void draw( Font& font );

    private:
        class Message final
        {
        public:
            void animate_text( rtl::wstring_view   text,
                               rtl::chrono::thirds exposition_duration
                               = rtl::chrono::thirds::zero() );

            void set_text( rtl::wstring_view text );

            void update( rtl::chrono::thirds time );
            void draw( Font& font, int x, int y );

        private:
            rtl::wstring text_to_hide;
            rtl::wstring text_to_show;

            enum class State
            {
                Idle,
                Update,
                Transition,
                Exposition
            };

            State m_state = State::Idle;

            rtl::chrono::thirds m_timeout_point;
            rtl::chrono::thirds m_transition_duration;
            rtl::chrono::thirds m_exposition_duration;

            float m_opacity_hide { 0.f };
            float m_opacity_show { 0.f };
        };

        Message                m_status;
        Message                m_message;
        rtl::array<Message, 5> m_stats;

        int m_screen_height { 0 };
    };
}
