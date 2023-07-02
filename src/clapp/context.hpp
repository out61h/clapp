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
#include <rtl/sys/application.hpp>
#include <rtl/sys/opencl.hpp>

namespace clapp
{
    class Context final
    {
    public:
        explicit Context( const rtl::opencl::device& device );
        ~Context() = default;

        void init( const rtl::Application::Input& input, unsigned gl_texture );
        void update( const rtl::Application::Input& input, rtl::Application::Output& output );

        void load_program( const wchar_t* filename );
        void load_program( rtl::string_view program );

        bool save_state( const wchar_t* filename );
        bool load_state( const wchar_t* filename );
        void reset_state();

        const rtl::string& opencl_device_name() const { return m_device_name; }

    private:
        static constexpr size_t keys_count = 256;

        // TODO: Use common (with OpenCL program) constants definitions
        static constexpr size_t state_buffer_size
            = ( 7680 / 4 ) * ( 4320 / 4 ) + 256 * 256 + 256 * 256;

        rtl::string          m_device_name;
        rtl::opencl::context m_context;
        rtl::opencl::program m_program;

        rtl::opencl::kernel m_kernel_input;
        rtl::opencl::kernel m_kernel_audio_out;
        rtl::opencl::kernel m_kernel_video_out;

        rtl::array<rtl::opencl::buffer, 2> m_buffer_state;
        size_t                             m_buffer_state_output_index { 0 };

        rtl::opencl::buffer m_buffer_keys;
        rtl::opencl::buffer m_buffer_audio_left;
        rtl::opencl::buffer m_buffer_audio_right;
        rtl::opencl::buffer m_buffer_video;

        rtl::vector<float> m_audio_data_left;
        rtl::vector<float> m_audio_data_right;
        int                m_audio_samples_generated { 0 };
    };
}
