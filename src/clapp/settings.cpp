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
#include <rtl/fix.hpp>
#include <rtl/fourcc.hpp>
#include <rtl/string.hpp>
#include <rtl/sys/debug.hpp>
#include <rtl/sys/filesystem.hpp>
#include <rtl/sys/opencl.hpp>
#include <rtl/sys/printf.hpp>

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

namespace
{
    constexpr rtl::string_view target_opencl_extension { "cl_khr_gl_sharing" };
}

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

namespace
{
    void center_window( HWND dialog_window )
    {
        [[maybe_unused]] BOOL result;

        HWND parent_window;
        RECT dialog_rect, parent_rect;

        if ( ( parent_window = ::GetParent( dialog_window ) ) != nullptr )
        {
            result = ::GetWindowRect( dialog_window, &dialog_rect );
            RTL_WINAPI_CHECK( result );

            result = ::GetWindowRect( parent_window, &parent_rect );
            RTL_WINAPI_CHECK( result );

            const int width  = dialog_rect.right - dialog_rect.left;
            const int height = dialog_rect.bottom - dialog_rect.top;

            int cx = ( ( parent_rect.right - parent_rect.left ) - width ) / 2 + parent_rect.left;
            int cy = ( ( parent_rect.bottom - parent_rect.top ) - height ) / 2 + parent_rect.top;

            const int screen_width  = ::GetSystemMetrics( SM_CXSCREEN );
            const int screen_height = ::GetSystemMetrics( SM_CYSCREEN );

            // Make sure that the dialog box never moves outside of the screen
            if ( cx < 0 )
                cx = 0;

            if ( cy < 0 )
                cy = 0;

            if ( cx + width > screen_width )
                cx = screen_width - width;

            if ( cy + height > screen_height )
                cy = screen_height - height;

            result = ::MoveWindow( dialog_window, cx, cy, width, height, FALSE );
            RTL_WINAPI_CHECK( result );
        }
    }

    template <typename T>
    T get_combobox_selected_item_data( HWND hwnd, int control_id )
    {
        LRESULT index = ::SendDlgItemMessageW( hwnd, control_id, CB_GETCURSEL, 0, 0 );
        RTL_ASSERT( index != CB_ERR );

        LRESULT data = ::SendDlgItemMessageW( hwnd,
                                              control_id,
                                              CB_GETITEMDATA,
                                              static_cast<WPARAM>( index ),
                                              0 );
        RTL_ASSERT( data != CB_ERR );

        return static_cast<T>( data );
    }

    template <typename T>
    unsigned add_combobox_item( HWND hwnd, int control_id, const wchar_t* string, T data )
    {
        LRESULT index = ::SendDlgItemMessageW( hwnd,
                                               control_id,
                                               CB_ADDSTRING,
                                               0,
                                               reinterpret_cast<LPARAM>( string ) );
        RTL_ASSERT( index >= 0 );

        [[maybe_unused]] LRESULT lresult = ::SendDlgItemMessageW( hwnd,
                                                                  control_id,
                                                                  CB_SETITEMDATA,
                                                                  static_cast<WPARAM>( index ),
                                                                  static_cast<LPARAM>( data ) );
        RTL_ASSERT( lresult != CB_ERR );

        return static_cast<unsigned>( index );
    }
}

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

        constexpr rtl::array<unsigned, 7>
            audio_rates { 8000, 11025, 16000, 22050, 32000, 44100, 48000 };

        for ( size_t i = 0; i < audio_rates.size(); ++i )
        {
            const unsigned rate = audio_rates[i];

            if ( rate <= target_audio_sample_rate )
                selection_index = i;

            // TODO: Use safe template-based rtl::wsprintf( "%u Hz", ... );
            // TODO: Take format string from resources
            const rtl::wstring text = rtl::to_wstring( rate ) + L" Hz";

            add_combobox_item( hwnd, control_id, text.c_str(), rate );
        }

        [[maybe_unused]] LRESULT lresult
            = ::SendDlgItemMessageW( hwnd, control_id, CB_SETCURSEL, selection_index, 0 );
        RTL_ASSERT( lresult != CB_ERR );
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

            add_combobox_item( hwnd, control_id, text.c_str(), count );
        }

        [[maybe_unused]] LRESULT lresult
            = ::SendDlgItemMessageW( hwnd, control_id, CB_SETCURSEL, selection_index, 0 );
        RTL_ASSERT( lresult != CB_ERR );
    }

    void init_opencl_device( HWND hwnd, int control_id )
    {
        [[maybe_unused]] LRESULT lresult;

        ::SendDlgItemMessageW( hwnd, control_id, CB_RESETCONTENT, 0, 0 );

        auto platforms     = rtl::opencl::platform::query_list();
        target_device_list = rtl::opencl::device::query_list( platforms );

        size_t selection_index = 0;

        for ( size_t i = 0; i < target_device_list.size(); ++i )
        {
            auto& device = target_device_list[i];

            if ( device.extension_supported( target_opencl_extension ) )
            {
                const rtl::wstring text = rtl::to_wstring( device.name() );

                const unsigned item_index = add_combobox_item( hwnd, control_id, text.c_str(), i );

                if ( device.name() == target_device_name )
                {
                    target_device_index = i;
                    selection_index     = item_index;
                }
            }
        }

        if ( target_device_list.empty() )
        {
            // TODO: Take string from resources
            lresult
                = ::SendDlgItemMessageW( hwnd, control_id, CB_ADDSTRING, 0, (LPARAM)L"Not found" );
            RTL_ASSERT( lresult >= 0 );

            HWND control_hwnd = ::GetDlgItem( hwnd, control_id );
            RTL_WINAPI_CHECK( control_hwnd != nullptr );

            ::EnableWindow( control_hwnd, FALSE );

            lresult = ::SendDlgItemMessageW( hwnd, control_id, CB_SETCURSEL, 0, 0 );
            RTL_ASSERT( lresult != CB_ERR );
        }
        else
        {
            target_device_name = target_device_list[target_device_index].name();

            lresult = ::SendDlgItemMessageW( hwnd, control_id, CB_SETCURSEL, selection_index, 0 );
            RTL_ASSERT( lresult != CB_ERR );
        }
    }

    void init( HWND hwnd )
    {
        // NOTE: The dialog will not appear on top of the fullscreen window without this code!
        [[maybe_unused]] BOOL result = ::SetWindowPos( hwnd,
                                                       HWND_TOP,
                                                       0,
                                                       0,
                                                       0,
                                                       0,
                                                       SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
        RTL_WINAPI_CHECK( result );

        ::SetForegroundWindow( hwnd );

        HWND control_hwnd = ::GetDlgItem( hwnd, IDOK );
        RTL_WINAPI_CHECK( control_hwnd != nullptr );
        ::SetFocus( control_hwnd );

        dialog_window = hwnd;

        init_audio_rate( hwnd, CLAPP_ID_CONTROL_AUDIO_RATE );
        init_audio_buffer_size( hwnd, CLAPP_ID_CONTROL_AUDIO_BUFFERS );
        init_opencl_device( hwnd, CLAPP_ID_CONTROL_OPENCL_DEVICE );
        update_max_latency( hwnd );
        update_opencl_device( hwnd );

        center_window( hwnd );
    }

    void free()
    {
        [[maybe_unused]] MMRESULT result = ::timeKillEvent( dialog_timer );
        RTL_ASSERT( result == TIMERR_NOERROR );

        dialog_timer  = 0;
        dialog_window = nullptr;
    }

    void update_opencl_device( HWND hwnd )
    {
        if ( target_device_list.empty() )
            return;

        const unsigned device_index
            = get_combobox_selected_item_data<unsigned>( hwnd, CLAPP_ID_CONTROL_OPENCL_DEVICE );

        const auto& device = target_device_list[device_index];

        [[maybe_unused]] BOOL result
            = ::SetDlgItemTextW( hwnd,
                                 CLAPP_ID_CONTROL_OPENCL_INFO,
                                 rtl::to_wstring( device.version() ).c_str() );
        RTL_WINAPI_CHECK( result );
    }

    void update_max_latency( HWND hwnd )
    {
        const unsigned audio_rate
            = get_combobox_selected_item_data<unsigned>( hwnd, CLAPP_ID_CONTROL_AUDIO_RATE );
        const unsigned buffers_count
            = get_combobox_selected_item_data<unsigned>( hwnd, CLAPP_ID_CONTROL_AUDIO_BUFFERS );

        const int audio_latency_ms
            = static_cast<int>( 1000 / target_monitor_frame_rate * buffers_count );
        const int audio_latency_samples
            = static_cast<int>( audio_rate / target_monitor_frame_rate * buffers_count );

        {
            // TODO: Use safe template-based rtl::wsprintf( "%i ms / %i samples", ... );
            // TODO: Take format string from resources
            const rtl::wstring text = rtl::to_wstring( audio_latency_ms ) + L" ms / "
                                    + rtl::to_wstring( audio_latency_samples ) + L" samples";

            [[maybe_unused]] BOOL result
                = ::SetDlgItemTextW( hwnd, CLAPP_ID_CONTROL_AUDIO_LATENCY, text.c_str() );
            RTL_WINAPI_CHECK( result );
        }

        {
            // TODO: Use safe template-based rtl::wsprintf( "%u fps", ... );
            // TODO: Take format string from resources
            const rtl::wstring text = rtl::to_wstring( target_monitor_frame_rate ) + L" fps";

            [[maybe_unused]] BOOL result
                = ::SetDlgItemTextW( hwnd, CLAPP_ID_CONTROL_FRAMERATE, text.c_str() );
            RTL_WINAPI_CHECK( result );
        }

        if ( dialog_timer )
        {
            [[maybe_unused]] MMRESULT result = ::timeKillEvent( dialog_timer );
            RTL_ASSERT( result == TIMERR_NOERROR );
        }

        dialog_timer
            = ::timeSetEvent( static_cast<UINT>( 1000 / target_monitor_frame_rate ),
                              0,
                              &Impl::on_timer,
                              reinterpret_cast<DWORD_PTR>( this ),
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
        using fp8 = rtl::fix<int, 8>;

        constexpr int step            = 20;
        constexpr int size            = 18;
        constexpr int period          = 1;
        constexpr int max_color_value = 255;
        constexpr int speed_factor    = 3;

        const int width    = banner_rect.right - banner_rect.left;
        const int height   = banner_rect.bottom - banner_rect.top;
        const int origin_x = banner_rect.left;
        const int origin_y = banner_rect.top;

        for ( int y = banner_rect.top; y < banner_rect.bottom; y += step )
        {
            const fp8 yy = fp8( y - origin_y ) / height;

            for ( int x = banner_rect.left; x < banner_rect.right; x += step )
            {
                const fp8 xx  = fp8( banner_phase + x - origin_x ) / width;
                const fp8 yy0 = rtl::abs( fp8( 1 ) - ( xx * ( 2 * period + 1 ) ) % 2 );

                const fp8 col = rtl::clamp( fp8( 1 ) - rtl::abs( yy - yy0 ), fp8( 0 ), fp8( 1 ) );
                const int val = static_cast<int>( col * max_color_value );

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

    bool update_target_settings( HWND hwnd )
    {
        if ( target_device_list.empty() )
        {
            // TODO: Take strings from resources
            [[maybe_unused]] int result = ::MessageBoxW( hwnd,
                                                         L"No OpenCL devices found in your system.",
                                                         L"Can't do anything useful",
                                                         MB_ICONWARNING | MB_OK );
            RTL_WINAPI_CHECK( result == IDOK );

            init_opencl_device( hwnd, CLAPP_ID_CONTROL_OPENCL_DEVICE );
            return false;
        }

        target_device_index
            = get_combobox_selected_item_data<unsigned>( hwnd, CLAPP_ID_CONTROL_OPENCL_DEVICE );
        target_audio_sample_rate
            = get_combobox_selected_item_data<unsigned>( hwnd, CLAPP_ID_CONTROL_AUDIO_RATE );
        target_audio_buffer_count
            = get_combobox_selected_item_data<unsigned>( hwnd, CLAPP_ID_CONTROL_AUDIO_BUFFERS );

        target_device_name = target_device_list[target_device_index].name();

        return true;
    }

    static INT_PTR CALLBACK DialogProc( HWND                    hwnd,
                                        UINT                    uMsg,
                                        [[maybe_unused]] WPARAM wParam,
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
                else if ( wm_id == CLAPP_ID_CONTROL_OPENCL_DEVICE )
                {
                    owner->update_opencl_device( hwnd );
                }
            }
            else
            {
                switch ( wm_id )
                {
                case IDOK:
                {
                    if ( owner->update_target_settings( hwnd ) )
                    {
                        [[maybe_unused]] BOOL result = ::EndDialog( hwnd, 1 );
                        RTL_WINAPI_CHECK( result );
                    }

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
            break;
        }
        }

        return 0;
    }
};

Settings::Settings()
    : m_impl( rtl::make_unique<Impl>() )
{
}

Settings::~Settings() = default;

bool Settings::setup( void* parent_window, unsigned display_framerate )
{
    m_impl->target_monitor_frame_rate = display_framerate;

    INT_PTR dlg_result = ::DialogBoxParamW( nullptr,
                                            MAKEINTRESOURCEW( CLAPP_ID_DIALOG_SETTINGS ),
                                            static_cast<HWND>( parent_window ),
                                            Impl::DialogProc,
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

unsigned Settings::target_audio_sample_rate() const
{
    return m_impl->target_audio_sample_rate;
}

unsigned Settings::target_audio_max_latency() const
{
    return m_impl->target_audio_sample_rate / m_impl->target_monitor_frame_rate
         * m_impl->target_audio_buffer_count;
}
