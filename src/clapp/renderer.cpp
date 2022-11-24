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
#include "renderer.hpp"

#pragma warning( push )
#pragma warning( disable : 4668 )
#include <Windows.h>
#include <gl/GL.h>
#pragma warning( pop )

// TODO: Check OpenGL errors?

using namespace clapp;

Renderer::~Renderer() { cleanup(); }

void Renderer::init( int width, int height )
{
    m_width  = width;
    m_height = height;

    ::glViewport( 0, 0, width, height );
    ::glDisable( GL_LIGHTING );
    ::glEnable( GL_TEXTURE_2D );
    ::glClearColor( 1.0f, 1.0f, 1.0f, 1.0f );

    cleanup();

    ::glGenTextures( 1, &m_texture );
    ::glBindTexture( GL_TEXTURE_2D, m_texture );
    ::glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    nullptr );
}

void Renderer::draw()
{
    // TODO: extract to renderer component
    // TODO: Use OpenGL 3.x API with shaders
    ::glClear( GL_COLOR_BUFFER_BIT );

    ::glMatrixMode( GL_PROJECTION );
    ::glLoadIdentity();

    ::glBindTexture( GL_TEXTURE_2D, m_texture );
    ::glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
    ::glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
    ::glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    ::glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

    ::glOrtho( 0.0, m_width, 0.0, m_height, -1.0, 1.0 );

    ::glMatrixMode( GL_MODELVIEW );
    ::glLoadIdentity();
    ::glBegin( GL_QUADS );
    ::glColor3f( 1.0f, 1.0f, 1.0f );
    ::glTexCoord2f( 0.f, 0.f );
    ::glVertex2i( 0, m_height );
    ::glTexCoord2f( 1.f, 0.f );
    ::glVertex2i( m_width, m_height );
    ::glTexCoord2f( 1.f, 1.f );
    ::glVertex2i( m_width, 0 );
    ::glTexCoord2f( 0.f, 1.f );
    ::glVertex2i( 0, 0 );
    ::glEnd();

    ::glFlush();
}

void Renderer::cleanup()
{
    if ( ::glIsTexture( m_texture ) )
    {
        ::glDeleteTextures( 1, &m_texture );
        m_texture = 0;
    }
}
