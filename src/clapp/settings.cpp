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
#include "settings.hpp"

#include <clapp.h>

#include <rtl/array.hpp>
#include <rtl/fourcc.hpp>
#include <rtl/string.hpp>
#include <rtl/sys/debug.hpp>
#include <rtl/sys/filesystem.hpp>
#include <rtl/sys/opencl.hpp>

// HACK: Shortcut to get RTL_WINAPI_CHECK macro definition. I plan to move all Windows-specific code
// to RTL library, so such solution is acceptable for a while.
#pragma warning( push )
#pragma warning( disable : 4668 )
#define RTL_IMPLEMENTATION
#define RTL_IMPL_DISABLE_WINDOWS_DEFS_FILTER
#include <rtl/sys/impl/win.hpp>
#undef RTL_IMPLEMENTATION
#undef RTL_IMPL_DISABLE_WINDOWS_DEFS_FILTER
#pragma warning( pop )

using namespace clapp;

namespace fs = rtl::filesystem;
using fs::file;

#pragma pack( push, 1 )
namespace format
{
    namespace signatures
    {
        constexpr rtl::uint32_t clap = rtl::make_fourcc( 'C', 'L', 'A', 'P' );
        constexpr rtl::uint32_t ocld = rtl::make_fourcc( 'O', 'C', 'L', 'D' );
        constexpr rtl::uint32_t adio = rtl::make_fourcc( 'A', 'D', 'I', 'O' );
    }

    namespace versions
    {
        constexpr rtl::uint32_t v1 = 1;
    }

    struct riff
    {
        rtl::uint32_t id;
        rtl::uint32_t size;
    };

    struct version
    {
        rtl::uint32_t version;
    };

    struct audio
    {
        rtl::uint32_t sample_rate;
        rtl::uint32_t buffer_count;
    };
}
#pragma pack( pop )

class Settings::Impl
{
public:
    unsigned                 target_audio_sample_rate { 48000 };
    unsigned                 target_audio_buffer_count { 4 };
    rtl::opencl::device_list target_device_list;
    unsigned                 target_device_index { 0 };
    rtl::string              target_device_name;
    unsigned                 target_monitor_frame_rate { 0 };

    HWND dialog_window { nullptr };
    UINT dialog_timer { 0 };
    int  banner_phase { 0 };

    void init_audio_rate( HWND hwnd, int control_id )
    {
        size_t selection_index = 0;

        constexpr rtl::array<unsigned, 7> audio_rates { 8000,  11025, 16000, 22050,
                                                        32000, 44100, 48000 };

        for ( size_t i = 0; i < audio_rates.size(); ++i )
        {
            const unsigned rate = audio_rates[i];

            if ( rate <= target_audio_sample_rate )
                selection_index = i;

            const rtl::wstring text = rtl::to_wstring( rate ) + L" Hz";

            ::SendDlgItemMessageW( hwnd, control_id, CB_ADDSTRING, 0, (LPARAM)text.c_str() );
            ::SendDlgItemMessageW( hwnd, control_id, CB_SETITEMDATA, i, (LPARAM)rate );
        }

        ::SendDlgItemMessageW( hwnd, control_id, CB_SETCURSEL, selection_index, 0 );
    }

    void init_audio_buffer_size( HWND hwnd, int control_id )
    {
        size_t selection_index = 0;

        constexpr rtl::array<unsigned, 6> buffer_count { 2, 3, 4, 5, 6, 8 };

        for ( size_t i = 0; i < buffer_count.size(); ++i )
        {
            const unsigned count = buffer_count[i];

            if ( count <= target_audio_buffer_count )
                selection_index = i;

            const rtl::wstring text = rtl::to_wstring( count );

            ::SendDlgItemMessageW( hwnd, control_id, CB_ADDSTRING, 0, (LPARAM)text.c_str() );
            ::SendDlgItemMessageW( hwnd, control_id, CB_SETITEMDATA, i, (LPARAM)count );
        }

        ::SendDlgItemMessageW( hwnd, control_id, CB_SETCURSEL, selection_index, 0 );
    }

    void init_opencl_device( HWND hwnd, int control_id )
    {
        // auto platforms = cl::platform::query_list_by_extension( "cl_khr_gl_sharing" );
        auto platforms     = rtl::opencl::platform::query_list();
        target_device_list = rtl::opencl::device::query_list( platforms );

        size_t selection_index = 0;

        for ( size_t i = 0; i < target_device_list.size(); ++i )
        {
            auto& device = target_device_list[i];

            if ( device.name() == target_device_name )
                selection_index = i;

            ::SendDlgItemMessageA( hwnd, control_id, CB_ADDSTRING, 0,
                                   (LPARAM)device.name().c_str() );
        }

        target_device_index = selection_index;
        target_device_name  = target_device_list[selection_index].name();
        ::SendDlgItemMessageA( hwnd, control_id, CB_SETCURSEL, selection_index, 0 );
    }

    void init( HWND hwnd )
    {
        // NOTE: The dialog will not appear on top of the fullscreen window without this code!
        [[maybe_unused]] BOOL result = ::SetWindowPos( hwnd, HWND_TOP, 0, 0, 0, 0,
                                                       SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
        RTL_WINAPI_CHECK( result );

        ::SetForegroundWindow( hwnd );
        ::SetFocus( hwnd );

        dialog_window = hwnd;

        init_audio_rate( hwnd, CLAPP_ID_CONTROL_AUDIO_RATE );
        init_audio_buffer_size( hwnd, CLAPP_ID_CONTROL_AUDIO_BUFFERS );
        init_opencl_device( hwnd, CLAPP_ID_CONTROL_OPENCL_DEVICE );
        update_max_latency( hwnd );
    }

    void free()
    {
        [[maybe_unused]] MMRESULT result = ::timeKillEvent( dialog_timer );
        RTL_ASSERT( result == TIMERR_NOERROR );

        dialog_timer  = 0;
        dialog_window = nullptr;
    }

    void update_max_latency( HWND hwnd )
    {
        const LONG_PTR rate_index
            = ::SendDlgItemMessageW( hwnd, CLAPP_ID_CONTROL_AUDIO_RATE, CB_GETCURSEL, 0, 0 );
        const LONG_PTR rate_value = ::SendDlgItemMessageW( hwnd, CLAPP_ID_CONTROL_AUDIO_RATE,
                                                           CB_GETITEMDATA, (WPARAM)rate_index, 0 );

        const LONG_PTR buffers_index
            = ::SendDlgItemMessageW( hwnd, CLAPP_ID_CONTROL_AUDIO_BUFFERS, CB_GETCURSEL, 0, 0 );
        const LONG_PTR buffers_value = ::SendDlgItemMessageW(
            hwnd, CLAPP_ID_CONTROL_AUDIO_BUFFERS, CB_GETITEMDATA, (WPARAM)buffers_index, 0 );

        const int audio_latency_ms
            = static_cast<int>( 1000 / target_monitor_frame_rate * buffers_value );
        const int audio_latency_samples
            = static_cast<int>( rate_value / target_monitor_frame_rate * buffers_value );
        {
            const rtl::wstring text = rtl::to_wstring( audio_latency_ms ) + L" ms / "
                + rtl::to_wstring( audio_latency_samples ) + L" samples";

            ::SetDlgItemTextW( hwnd, CLAPP_ID_CONTROL_AUDIO_LATENCY, text.c_str() );
        }

        {
            const rtl::wstring text = rtl::to_wstring( target_monitor_frame_rate ) + L" fps";
            ::SetDlgItemTextW( hwnd, CLAPP_ID_CONTROL_FRAMERATE, text.c_str() );
        }

        [[maybe_unused]] MMRESULT result;

        if ( dialog_timer )
        {
            result = ::timeKillEvent( dialog_timer );
            RTL_ASSERT( result == TIMERR_NOERROR );
        }

        dialog_timer
            = ::timeSetEvent( static_cast<UINT>( 1000 / target_monitor_frame_rate ), 0,
                              &Impl::on_timer, reinterpret_cast<DWORD_PTR>( this ),
                              TIME_PERIODIC | TIME_CALLBACK_FUNCTION | TIME_KILL_SYNCHRONOUS );
        RTL_ASSERT( dialog_timer != 0 );
    }

    static void CALLBACK on_timer( UINT, UINT, DWORD_PTR dwUser, DWORD_PTR, DWORD_PTR )
    {
        Impl* impl = reinterpret_cast<Impl*>( dwUser );
        impl->update_banner( impl->dialog_window );
    }

    void update_banner( HWND hwnd )
    {
        HWND banner = ::GetDlgItem( hwnd, CLAPP_ID_CONTROL_BANNER );
        RTL_WINAPI_CHECK( banner != nullptr );

        HDC dc = ::GetWindowDC( banner );
        RTL_WINAPI_CHECK( dc != nullptr );

        [[maybe_unused]] BOOL result;

        RECT rect;
        result = ::GetClientRect( banner, &rect );
        RTL_WINAPI_CHECK( result );

        draw_banner( dc, rect );

        ::ReleaseDC( banner, dc );
    }

    void draw_banner( HDC banner_dc, const RECT& banner_rect )
    {
        constexpr int step            = 20;
        constexpr int size            = 18;
        constexpr int period          = 10;
        constexpr int half_period     = period / 2;
        constexpr int denominator     = 256;
        constexpr int max_color_value = 255;
        constexpr int speed_factor    = 3;

        const int width    = banner_rect.right - banner_rect.left;
        const int height   = banner_rect.top - banner_rect.bottom;
        const int origin_x = banner_rect.left;
        const int origin_y = banner_rect.top;

        // TODO: skew
        for ( int y = banner_rect.top; y < banner_rect.bottom; y += step )
        {
            const int yy = denominator * ( y - origin_y ) / height;

            for ( int x = banner_rect.left; x < banner_rect.right; x += step )
            {
                const int yy0 = denominator
                    * rtl::abs( half_period
                                - ( period * ( banner_phase + x - origin_x ) / width ) % period )
                    / half_period;

                const int val = rtl::clamp( denominator - rtl::abs( denominator + yy - yy0 ), 0,
                                            max_color_value );

                HBRUSH brush = ::CreateSolidBrush( RGB( val, val / 2, max_color_value - val / 4 ) );
                RTL_WINAPI_CHECK( brush != nullptr );

                [[maybe_unused]] BOOL result;

                RECT rect;
                result = ::SetRect( &rect, x, y, x + size, y + size );
                RTL_WINAPI_CHECK( result );
                result = ::FillRect( banner_dc, &rect, brush );
                RTL_WINAPI_CHECK( result );

                result = ::DeleteObject( brush );
                RTL_WINAPI_CHECK( result );
            }
        }

        banner_phase += ( width / step ) / speed_factor;
    }

    void update_target_settings( HWND hwnd )
    {
        {
            target_device_index = static_cast<unsigned>(
                ::SendDlgItemMessageW( hwnd, CLAPP_ID_CONTROL_OPENCL_DEVICE, CB_GETCURSEL, 0, 0 ) );
            target_device_name = target_device_list[target_device_index].name();
        }

        {
            const unsigned audio_rate_index = static_cast<unsigned>(
                ::SendDlgItemMessageW( hwnd, CLAPP_ID_CONTROL_AUDIO_RATE, CB_GETCURSEL, 0, 0 ) );

            target_audio_sample_rate = static_cast<unsigned>( ::SendDlgItemMessageW(
                hwnd, CLAPP_ID_CONTROL_AUDIO_RATE, CB_GETITEMDATA, audio_rate_index, 0 ) );
        }

        {
            const unsigned audio_buffers_count_index = static_cast<unsigned>(
                ::SendDlgItemMessageW( hwnd, CLAPP_ID_CONTROL_AUDIO_BUFFERS, CB_GETCURSEL, 0, 0 ) );

            target_audio_buffer_count = static_cast<unsigned>(
                ::SendDlgItemMessageW( hwnd, CLAPP_ID_CONTROL_AUDIO_BUFFERS, CB_GETITEMDATA,
                                       audio_buffers_count_index, 0 ) );
        }
    }

    static INT_PTR CALLBACK DialogProc( HWND hwnd, UINT uMsg, [[maybe_unused]] WPARAM wParam,
                                        [[maybe_unused]] LPARAM lParam )
    {
        Impl* owner = reinterpret_cast<Impl*>( ::GetWindowLongPtrW( hwnd, GWLP_USERDATA ) );

        switch ( uMsg )
        {
        case WM_COMMAND:
        {
            const auto wm_id    = LOWORD( wParam );
            const auto wm_event = HIWORD( wParam );

            if ( wm_event == CBN_SELCHANGE )
            {
                if ( wm_id == CLAPP_ID_CONTROL_AUDIO_RATE
                     || wm_id == CLAPP_ID_CONTROL_AUDIO_BUFFERS )
                {
                    owner->update_max_latency( hwnd );
                }
            }
            else
            {
                switch ( wm_id )
                {
                case IDOK:
                {
                    owner->update_target_settings( hwnd );
                    [[maybe_unused]] BOOL result = ::EndDialog( hwnd, 1 );
                    RTL_WINAPI_CHECK( result );
                    break;
                }

                case IDCANCEL:
                    [[maybe_unused]] BOOL result = ::EndDialog( hwnd, 0 );
                    RTL_WINAPI_CHECK( result );
                    break;
                }

                break;
            }

            break;
        }

        case WM_DESTROY:
        {
            owner->free();
            break;
        }

        case WM_INITDIALOG:
        {
            Impl* that = reinterpret_cast<Impl*>( lParam );
            RTL_ASSERT( that != nullptr );

            ::SetWindowLongPtrW( hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( that ) );

            that->init( hwnd );

#if 0 // TODO: center
        CenterWindow( hWndDlg );

        RECT rect;
        RTL_WIN32_ASSERT( ::GetWindowRect( hWndDlg, &rect ) );
        HWND parent = ::GetParent( hWndDlg );
        RTL_WIN32_ASSERT( ::GetWindowRect( hWndDlg, &rect ) );

        if ( parent )
        {
            RECT parent_rect;
            RTL_WIN32_ASSERT( ::GetWindowRect( parent, &parent_rect ) );

            RECT rc;
            RTL_WIN32_ASSERT( ::CopyRect( &rc, &parent_rect ) );

            RTL_WIN32_ASSERT( ::OffsetRect( &rect, -rect.left, -rect.top ) );

            RTL_WIN32_ASSERT( ::OffsetRect( &rc, -rc.left, -rc.top ) );

            RTL_WIN32_ASSERT( ::OffsetRect( &rc, -rect.right, -rect.bottom ) );

            RTL_WIN32_ASSERT( ::SetWindowPos( hWndDlg, HWND_TOP,
                                              parent_rect.left + ( rc.right / 2 ),
                                              parent_rect.top + ( rc.bottom / 2 ), 0, 0,
                                              SWP_NOSIZE /* | SWP_NOZORDER */ | SWP_SHOWWINDOW ) );
        }
#endif

            break;
        }
        }

        return 0;
    }
};

Settings::Settings()
    : m_impl( new Impl ) // TODO: Implement rtl::make_unique
{
}

Settings::~Settings() = default;

bool Settings::setup( void* parent_window, unsigned display_framerate )
{
    m_impl->target_monitor_frame_rate = display_framerate;

    INT_PTR dlg_result = ::DialogBoxParamW( nullptr, MAKEINTRESOURCEW( CLAPP_ID_DIALOG_SETTINGS ),
                                            static_cast<HWND>( parent_window ), Impl::DialogProc,
                                            reinterpret_cast<LPARAM>( m_impl.get() ) );
    RTL_WINAPI_CHECK( dlg_result >= 0 );
    return dlg_result > 0;
}

void Settings::load( const wchar_t* filename )
{
    auto f = file::open( filename, file::access::read_only, file::mode::open_existing );
    if ( !f )
        return;

    [[maybe_unused]] unsigned read_bytes;
    {
        format::riff header { 0 };
        read_bytes = f.read( &header, sizeof( header ) );
        RTL_ASSERT( read_bytes == sizeof( header ) );

        if ( header.id != format::signatures::clap )
            return;

        if ( header.size != sizeof( format::version ) )
            return;

        format::version version { 0 };
        read_bytes = f.read( &version, sizeof( version ) );
        RTL_ASSERT( read_bytes == sizeof( version ) );

        if ( version.version != format::versions::v1 )
            return;
    }

    {
        format::riff header { 0 };
        read_bytes = f.read( &header, sizeof( header ) );
        RTL_ASSERT( read_bytes == sizeof( header ) );

        if ( header.id != format::signatures::ocld )
            return;

        m_impl->target_device_name = rtl::string( header.size, 0 );
        read_bytes = f.read( m_impl->target_device_name.data(), m_impl->target_device_name.size() );
        RTL_ASSERT( read_bytes == m_impl->target_device_name.size() );
    }

    {
        format::riff header { 0 };

        read_bytes = f.read( &header, sizeof( header ) );
        RTL_ASSERT( read_bytes == sizeof( header ) );

        if ( header.id != format::signatures::adio )
            return;

        if ( header.size != sizeof( format::audio ) )
            return;

        format::audio audio { 0 };
        read_bytes = f.read( &audio, sizeof( audio ) );
        RTL_ASSERT( read_bytes == sizeof( audio ) );

        m_impl->target_audio_sample_rate  = audio.sample_rate;
        m_impl->target_audio_buffer_count = audio.buffer_count;
    }
}

void Settings::save( const wchar_t* filename )
{
    auto f = file::open( filename, file::access::write_only, file::mode::create_always );
    if ( !f )
        return;

    {
        format::riff header;
        header.id   = format::signatures::clap;
        header.size = sizeof( format::version );

        format::version version;
        version.version = format::versions::v1;

        f.write( &header, sizeof( header ) );
        f.write( &version, sizeof( version ) );
    }

    {
        format::riff header;
        header.id   = format::signatures::ocld;
        header.size = m_impl->target_device_name.size();

        f.write( &header, sizeof( header ) );
        f.write( m_impl->target_device_name.data(), m_impl->target_device_name.size() );
    }

    {
        format::riff header;
        header.id   = format::signatures::adio;
        header.size = sizeof( format::audio );

        format::audio audio;
        audio.sample_rate  = m_impl->target_audio_sample_rate;
        audio.buffer_count = m_impl->target_audio_buffer_count;

        f.write( &header, sizeof( header ) );
        f.write( &audio, sizeof( audio ) );
    }
}

const rtl::opencl::device& Settings::target_opencl_device() const
{
    return m_impl->target_device_list[m_impl->target_device_index];
}

unsigned Settings::target_audio_sample_rate() const { return m_impl->target_audio_sample_rate; }

unsigned Settings::target_audio_max_latency() const
{
    return m_impl->target_audio_sample_rate / m_impl->target_monitor_frame_rate
        * m_impl->target_audio_buffer_count;
}

// TODO: display OpenCL device status and info - extension
// TODO: update fps after moving to another monitor
// TODO: if no opencl - display error and disable OK button
