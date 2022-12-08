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
#include "hud.hpp"
#include "font.hpp"

namespace timings
{
    constexpr rtl::chrono::milliseconds transition { 300 };
    constexpr rtl::chrono::milliseconds exposition { 3000 };
}

using namespace clapp;

void Hud::init( int /* screen_width */, int screen_height )
{
    m_screen_height = screen_height;
}

void Hud::set_status( rtl::wstring_view text )
{
    m_status.set_text( text );
}

void Hud::add_message( rtl::wstring_view text )
{
    m_message.set_text( text, timings::exposition );
}

void Hud::update( rtl::chrono::thirds time )
{
    m_status.update( time );
    m_message.update( time );
}

void Hud::draw( Font& font )
{
    m_status.draw( font, font.size(), font.size() );
    m_message.draw( font, font.size(), m_screen_height - font.size() );
}

void Hud::Message::set_text( rtl::wstring_view text, rtl::chrono::thirds exposition_duration )
{
    text_to_hide = rtl::move( text_to_show );
    text_to_show = text;

    m_opacity_hide = m_opacity_show;
    m_opacity_show = 0.f;

    m_state = State::Update;

    m_exposition_duration = exposition_duration;
}

void Hud::Message::update( rtl::chrono::thirds time )
{
    switch ( m_state )
    {
    case State::Idle:
        break;

    case State::Update:
        m_state               = State::Transition;
        m_transition_duration = timings::transition;
        m_timeout_point       = time + m_transition_duration;
        break;

    case State::Transition:
        if ( time >= m_timeout_point )
        {
            m_opacity_show = 1.f;
            m_opacity_hide = 0.f;

            if ( m_exposition_duration.count() > 0 )
            {
                m_state         = State::Exposition;
                m_timeout_point = time + m_exposition_duration;
            }
            else
            {
                m_state = State::Idle;
            }
        }
        else
        {
            m_opacity_hide = static_cast<float>( ( m_timeout_point - time ).count() )
                           / m_transition_duration.count();
            m_opacity_show = 1.f - m_opacity_hide;
        }

        break;

    case State::Exposition:
        if ( time >= m_timeout_point )
        {
            text_to_hide = rtl::move( text_to_show );
            text_to_show.clear();

            m_opacity_hide = m_opacity_show;
            m_opacity_show = 0.f;

            m_state = State::Transition;

            m_exposition_duration = rtl::chrono::thirds::zero();
            m_transition_duration = timings::transition;
            m_timeout_point       = time + m_transition_duration;
        }

        break;
    }
}

void Hud::Message::draw( Font& font, int x, int y )
{
    font.draw( text_to_hide, x, y, m_opacity_hide );
    font.draw( text_to_show, x, y, m_opacity_show );
}
