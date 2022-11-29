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

#pragma warning( push )
#pragma warning( disable : 4668 )
#include <Windows.h>
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

    int banner_phase { 0 };

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
        // NOTE: Without this, the dialog will not appear on top of the fullscreen window!
        ::SetWindowPos( hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
        ::SetForegroundWindow( hwnd );
        ::SetFocus( hwnd );

        init_audio_rate( hwnd, CLAPP_ID_CONTROL_AUDIO_RATE );
        init_audio_buffer_size( hwnd, CLAPP_ID_CONTROL_AUDIO_BUFFERS );
        init_opencl_device( hwnd, CLAPP_ID_CONTROL_OPENCL_DEVICE );
        update_max_latency( hwnd );
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

        const int audio_latency
            = static_cast<int>( 1000 / target_monitor_frame_rate * buffers_value );
        {
            const rtl::wstring text = rtl::to_wstring( audio_latency ) + L" ms";

            ::SetDlgItemTextW( hwnd, CLAPP_ID_CONTROL_AUDIO_LATENCY, text.c_str() );
        }

        {
            const rtl::wstring text = rtl::to_wstring( target_monitor_frame_rate ) + L" fps";
            ::SetDlgItemTextW( hwnd, CLAPP_ID_CONTROL_FRAMERATE, text.c_str() );
        }

        ::SetTimer( hwnd, 1, static_cast<UINT>( 1000 / target_monitor_frame_rate ),
                    0 ); // TODO: check result

        // TODO: kill timer?
    }

    void update_banner( HWND hwnd )
    {
        // TODO: animation freezes when combobox is opening

        HWND banner = ::GetDlgItem( hwnd, CLAPP_ID_CONTROL_BANNER ); // TODO: check result
        ::InvalidateRect( banner, nullptr, FALSE ); // TODO: check result
        banner_phase += 11;
        ::SetTimer( hwnd, 1, static_cast<UINT>( 1000 / target_monitor_frame_rate ),
                    0 ); // TODO: check result

        // TODO: kill timer?
    }

    void draw_banner( DRAWITEMSTRUCT* dis )
    {
        constexpr int step   = 20;
        constexpr int size   = 18;
        int           offset = banner_phase; // TODO: simplify

        const int width    = dis->rcItem.right - dis->rcItem.left;
        const int origin_x = dis->rcItem.left;

        for ( int y = dis->rcItem.top; y < dis->rcItem.bottom; y += step )
        {
            const int yy = 255 * ( y - dis->rcItem.top ) / ( dis->rcItem.top - dis->rcItem.bottom );

            for ( int x = dis->rcItem.left; x < dis->rcItem.right; x += step )
            {
                const int yy0
                    = 255 * rtl::abs( 5 - ( 10 * ( offset + x - origin_x ) / width ) % 10 ) / 5;

                const int val = ( 255 - rtl::abs( 255 + yy - yy0 ) ) & 0xff;

                HBRUSH brush = ::CreateSolidBrush(
                    RGB( val, val / 2, 255 - val / 4 ) ); // TODO: check result

                RECT rect;
                ::SetRect( &rect, x, y, x + size, y + size ); // TODO: check result
                ::FillRect( dis->hDC, &rect, brush ); // TODO: check result

                ::DeleteObject( brush ); // TODO: check result
            }
        }
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
        case WM_DRAWITEM:
        {
            if ( wParam == CLAPP_ID_CONTROL_BANNER )
            {
                DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>( lParam );
                owner->draw_banner( dis );
            }

            break;
        }

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
                    ::EndDialog( hwnd, 1 ); // TODO: check result
                    break;
                }

                case IDCANCEL:
                    ::EndDialog( hwnd, 0 ); // TODO: check result
                    break;
                }

                break;
            }

            break;
        }

        case WM_TIMER:
        {
            owner->update_banner( hwnd );
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

    // TODO: check dlg_result >= 0
    return dlg_result > 0;
}

void Settings::load( const wchar_t* filename )
{
    auto f = file::open( filename, file::access::read_only, file::mode::open_existing );
    if ( !f )
        return;

    {
        format::riff header { 0 };
        f.read( &header, sizeof( header ) );

        if ( header.id != format::signatures::clap )
            return;

        if ( header.size != sizeof( format::version ) )
            return;

        format::version version { 0 };
        f.read( &version, sizeof( version ) );

        if ( version.version != format::versions::v1 )
            return;
    }

    {
        format::riff header { 0 };
        f.read( &header, sizeof( header ) );

        if ( header.id != format::signatures::ocld )
            return;

        m_impl->target_device_name = rtl::string( header.size, 0 );
        f.read( m_impl->target_device_name.data(), m_impl->target_device_name.size() );
    }

    {
        format::riff header { 0 };

        f.read( &header, sizeof( header ) );

        if ( header.id != format::signatures::adio )
            return;

        if ( header.size != sizeof( format::audio ) )
            return;

        format::audio audio { 0 };
        f.read( &audio, sizeof( audio ) );
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

// TODO: display audio buffer length
// TODO: display OpenCL device status and info - extension
// TODO: update fps after moving to another monitor
// TODO: if no opencl - show warnigns and exit
