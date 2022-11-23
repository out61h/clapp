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
        Context( const rtl::opencl::device& device, rtl::string_view program,
                 const rtl::uint32_t* cdata, size_t cdata_size, rtl::vector<rtl::uint32_t> state );

        ~Context() = default;

        void init( const rtl::application::input& input, unsigned gl_texture );
        void update( const rtl::application::input& input, rtl::application::output& output );

        rtl::vector<rtl::uint32_t> save();

        void load( const rtl::vector<rtl::uint32_t>& state );

    private:
        static constexpr size_t keys_count = 256;

        // TODO: explain calculation
        static constexpr size_t state_buffer_size
            = ( 2560 / 4 ) * ( 1440 / 4 ) + 256 * 256 + 256 * 256;

        // TODO: add m_ prefix
        rtl::opencl::context context;
        rtl::opencl::program program;

        rtl::opencl::kernel kernel_input;
        rtl::opencl::kernel kernel_audio_out;
        rtl::opencl::kernel kernel_video_out;

        rtl::opencl::buffer buffer_cdata;
        rtl::opencl::buffer buffer_tdata;

        rtl::array<rtl::opencl::buffer, 2> buffer_state;
        size_t                             buffer_state_output_index { 0 };

        rtl::opencl::buffer buffer_keys;
        rtl::opencl::buffer buffer_audio_left;
        rtl::opencl::buffer buffer_audio_right;
        rtl::opencl::buffer buffer_video;

        rtl::vector<float> audio_data_left;
        rtl::vector<float> audio_data_right;
        int                audio_samples_generated { 0 };
    };
}
