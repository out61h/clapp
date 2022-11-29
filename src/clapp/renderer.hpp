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

namespace clapp
{
    class Renderer final
    {
    public:
        Renderer() = default;
        ~Renderer();

        void init( int width, int height );
        void draw();
        void clear();

        unsigned texture() const { return m_texture; }

    private:
        Renderer( const Renderer& )            = delete;
        Renderer& operator=( const Renderer& ) = delete;

        void cleanup();

        unsigned m_texture { 0 };
        int      m_width { 0 };
        int      m_height { 0 };
    };
}
