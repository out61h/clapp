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

#include <rtl/string.hpp>

namespace clapp
{
    class Font final
    {
    public:
        explicit Font( int size );
        ~Font();

        void draw( rtl::wstring_view text, int x, int y, float opacity );

        int size() const { return m_font_size; }

    private:
        int      m_font_size { 0 };
        unsigned m_font_lists { 0 };
    };
}
