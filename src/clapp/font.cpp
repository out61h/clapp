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
#include "font.hpp"

#pragma warning( push )
#pragma warning( disable : 4668 )
#define NOMINMAX
#include <Windows.h>
#include <gl/GL.h>
#pragma warning( pop )

// TODO: Check OpenGL errors?
// TODO: Check Win errors

using namespace clapp;

namespace
{
    // TODO: Add Unicode support
    // constexpr size_t glyph_count = 65536;
    constexpr size_t glyph_count = 256;
}

Font::Font( int size )
    : m_font_size( size )
{
    HDC hdc = ::wglGetCurrentDC();

    HFONT font = ::CreateFontW( m_font_size,
                                0,
                                0,
                                0,
                                FW_DONTCARE,
                                FALSE,
                                FALSE,
                                FALSE,
                                DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS,
                                DEFAULT_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE,
                                nullptr );

    ::SelectObject( hdc, font );

    m_font_lists = ::glGenLists( glyph_count );

    ::wglUseFontBitmapsA( hdc, 0, glyph_count, m_font_lists );

    [[maybe_unused]] BOOL result = ::DeleteObject( font );
}

void Font::draw( rtl::wstring_view text, int x, int y, float opacity )
{
    ::glEnable( GL_BLEND );
    ::glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    ::glColor4f( 1.0f, 0.0f, 1.0f, opacity );
    ::glRasterPos3i( x, y, 0 );
    ::glListBase( m_font_lists );
    ::glCallLists( static_cast<GLsizei>( text.size() ), GL_UNSIGNED_SHORT, text.data() );
    ::glDisable( GL_BLEND );
}

Font::~Font()
{
    ::glDeleteLists( m_font_lists, glyph_count );
}
