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
#include <rtl/sys/opencl.hpp>

namespace clapp
{
    class Settings final
    {
    public:
        Settings();
        ~Settings();

        void load( const wchar_t* filename );
        void save( const wchar_t* filename );
        bool setup( void* parent_window, unsigned display_framerate );

        const rtl::opencl::device& target_opencl_device() const;
        unsigned                   target_audio_sample_rate() const;
        unsigned                   target_audio_max_latency() const;

    private:
        class Impl;
        rtl::unique_ptr<Impl> m_impl;
    };
}
