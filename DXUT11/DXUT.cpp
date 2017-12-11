//--------------------------------------------------------------------------------------
// File: DXUT.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include <atlstr.h>
#define DXUT_MIN_WINDOW_SIZE_X 200
#define DXUT_MIN_WINDOW_SIZE_Y 200
#define DXUT_COUNTER_STAT_LENGTH 2048


#ifndef ARRAYSIZE
extern "C++" // templates cannot be declared to have 'C' linkage
template <typename T, size_t N>
char (*RtlpNumberOf( UNALIGNED T (&)[N] ))[N];

#define RTL_NUMBER_OF_V2(A) (sizeof(*RtlpNumberOf(A)))
#define ARRAYSIZE(A)    RTL_NUMBER_OF_V2(A)
#endif

//--------------------------------------------------------------------------------------
// Thread safety
//--------------------------------------------------------------------------------------
CRITICAL_SECTION    g_cs;
bool                g_bThreadSafe = true;


//--------------------------------------------------------------------------------------
// Automatically enters & leaves the CS upon object creation/deletion
//--------------------------------------------------------------------------------------
class DXUTLock
{
public:
    inline DXUTLock()  { if( g_bThreadSafe ) EnterCriticalSection( &g_cs ); }
    inline ~DXUTLock() { if( g_bThreadSafe ) LeaveCriticalSection( &g_cs ); }
};

//--------------------------------------------------------------------------------------
// Helper macros to build member functions that access member variables with thread safety
//--------------------------------------------------------------------------------------
#define SET_ACCESSOR( x, y )       inline void Set##y( x t )   { DXUTLock l; m_##y = t; };
#define GET_ACCESSOR( x, y )       inline x Get##y()           { DXUTLock l; return m_##y; };
#define GET_SET_ACCESSOR( x, y )   SET_ACCESSOR( x, y ) GET_ACCESSOR( x, y )

#define SETP_ACCESSOR( x, y )      inline void Set##y( x* t )  { DXUTLock l; m_##y = *t; };
#define GETP_ACCESSOR( x, y )      inline x* Get##y()          { DXUTLock l; return &m_##y; };
#define GETP_SETP_ACCESSOR( x, y ) SETP_ACCESSOR( x, y ) GETP_ACCESSOR( x, y )


//--------------------------------------------------------------------------------------
// Stores timer callback info
//--------------------------------------------------------------------------------------
struct DXUT_TIMER
{
    LPDXUTCALLBACKTIMER pCallbackTimer;
    void* pCallbackUserContext;
    float fTimeoutInSecs;
    float fCountdown;
    bool bEnabled;
    UINT nID;
};



//--------------------------------------------------------------------------------------
// Stores DXUT state and data access is done with thread safety (if g_bThreadSafe==true)
//--------------------------------------------------------------------------------------
class DXUTState
{
public:
   // struct STATE
   // {
        // D3D9 specific
        DXUTDeviceSettings*     m_CurrentDeviceSettings;   // current device settings
        D3DSURFACE_DESC         m_BackBufferSurfaceDesc9;  // D3D9 back buffer surface description

        // D3D11 specific
        IDXGIFactory1*           m_DXGIFactory;             // DXGI Factory object
        IDXGIAdapter1*           m_DXGIAdapter;            // The DXGI adapter object for the D3D11 device
        IDXGIOutput**           m_DXGIOutputArray;        // The array of output obj for the D3D11 adapter obj
        UINT                    m_DXGIOutputArraySize;    // Number of elements in m_D3D11OutputArray
        IDXGISwapChain*         m_DXGISwapChain;          // the D3D11 swapchain
        DXGI_SURFACE_DESC       m_BackBufferSurfaceDescDXGI; // D3D11 back buffer surface description
        bool                    m_RenderingOccluded;       // Rendering is occluded by another window
        bool                    m_DoNotStoreBufferSize;    // Do not store the buffer size on WM_SIZE messages

        // D3D11 specific
        bool                    m_D3D11Available;          // if true, then D3D11 is available 
        ID3D11Device*			m_D3D11Device;             // the D3D11 rendering device
        ID3D11DeviceContext*	m_D3D11DeviceContext;	   // the D3D11 immediate device context
        D3D_FEATURE_LEVEL		m_D3D11FeatureLevel;	   // the D3D11 feature level that this device supports
        ID3D11Texture2D*        m_D3D11DepthStencil;       // the D3D11 depth stencil texture (optional)
        ID3D11DepthStencilView* m_D3D11DepthStencilView;   // the D3D11 depth stencil view (optional)
        ID3D11RenderTargetView* m_D3D11RenderTargetView;   // the D3D11 render target view
        ID3D11RasterizerState*  m_D3D11RasterizerState;    // the D3D11 Rasterizer state

        // General
        HWND  m_HWNDFocus;                  // the main app focus window
        HWND  m_HWNDDeviceFullScreen;       // the main app device window in fullscreen mode
        HWND  m_HWNDDeviceWindowed;         // the main app device window in windowed mode
        HMONITOR m_AdapterMonitor;          // the monitor of the adapter 
        HMENU m_Menu;                       // handle to menu

        UINT m_FullScreenBackBufferWidthAtModeChange;  // back buffer size of fullscreen mode right before switching to windowed mode.  Used to restore to same resolution when toggling back to fullscreen
        UINT m_FullScreenBackBufferHeightAtModeChange; // back buffer size of fullscreen mode right before switching to windowed mode.  Used to restore to same resolution when toggling back to fullscreen
        UINT m_WindowBackBufferWidthAtModeChange;  // back buffer size of windowed mode right before switching to fullscreen mode.  Used to restore to same resolution when toggling back to windowed mode
        UINT m_WindowBackBufferHeightAtModeChange; // back buffer size of windowed mode right before switching to fullscreen mode.  Used to restore to same resolution when toggling back to windowed mode
        DWORD m_WindowedStyleAtModeChange;  // window style
        WINDOWPLACEMENT m_WindowedPlacement;// record of windowed HWND position/show state/etc
        bool  m_TopmostWhileWindowed;       // if true, the windowed HWND is topmost 
        bool  m_Minimized;                  // if true, the HWND is minimized
        bool  m_Maximized;                  // if true, the HWND is maximized
        bool  m_MinimizedWhileFullscreen;   // if true, the HWND is minimized due to a focus switch away when fullscreen mode
        bool  m_IgnoreSizeChange;           // if true, DXUT won't reset the device upon HWND size change

        double m_Time;                      // current time in seconds
        double m_AbsoluteTime;              // absolute time in seconds
        float m_ElapsedTime;                // time elapsed since last frame

        HINSTANCE m_HInstance;              // handle to the app instance
        double m_LastStatsUpdateTime;       // last time the stats were updated
        DWORD m_LastStatsUpdateFrames;      // frames count since last time the stats were updated
        float m_FPS;                        // frames per second
        int   m_CurrentFrameNumber;         // the current frame number
        HHOOK m_KeyboardHook;               // handle to keyboard hook
        bool  m_AllowShortcutKeysWhenFullscreen; // if true, when fullscreen enable shortcut keys (Windows keys, StickyKeys shortcut, ToggleKeys shortcut, FilterKeys shortcut) 
        bool  m_AllowShortcutKeysWhenWindowed;   // if true, when windowed enable shortcut keys (Windows keys, StickyKeys shortcut, ToggleKeys shortcut, FilterKeys shortcut) 
        bool  m_AllowShortcutKeys;          // if true, then shortcut keys are currently disabled (Windows key, etc)
        bool  m_CallDefWindowProc;          // if true, DXUTStaticWndProc will call DefWindowProc for unhandled messages. Applications rendering to a dialog may need to set this to false.
        STICKYKEYS m_StartupStickyKeys;     // StickyKey settings upon startup so they can be restored later
        TOGGLEKEYS m_StartupToggleKeys;     // ToggleKey settings upon startup so they can be restored later
        FILTERKEYS m_StartupFilterKeys;     // FilterKey settings upon startup so they can be restored later

        bool  m_AppSupportsD3D9Override;    // true if app sets via DXUTSetD3DVersionSupport()
        bool  m_AppSupportsD3D11Override;   // true if app sets via DXUTSetD3DVersionSupport()
        bool  m_UseD3DVersionOverride;      // true if the app ever calls DXUTSetD3DVersionSupport()

        bool  m_HandleEscape;               // if true, then DXUT will handle escape to quit
        bool  m_HandleAltEnter;             // if true, then DXUT will handle alt-enter to toggle fullscreen
        bool  m_HandlePause;                // if true, then DXUT will handle pause to toggle time pausing
        bool  m_ShowMsgBoxOnError;          // if true, then msgboxes are displayed upon errors
        bool  m_NoStats;                    // if true, then DXUTGetFrameStats() and DXUTGetDeviceStats() will return blank strings
        bool  m_ClipCursorWhenFullScreen;   // if true, then DXUT will keep the cursor from going outside the window when full screen
        bool  m_ShowCursorWhenFullScreen;   // if true, then DXUT will show a cursor when full screen
        bool  m_ConstantFrameTime;          // if true, then elapsed frame time will always be 0.05f seconds which is good for debugging or automated capture
        float m_TimePerFrame;               // the constant time per frame in seconds, only valid if m_ConstantFrameTime==true
        bool  m_WireframeMode;              // if true, then D3DRS_FILLMODE==D3DFILL_WIREFRAME else D3DRS_FILLMODE==D3DFILL_SOLID 
        bool  m_AutoChangeAdapter;          // if true, then the adapter will automatically change if the window is different monitor
        bool  m_WindowCreatedWithDefaultPositions; // if true, then CW_USEDEFAULT was used and the window should be moved to the right adapter
        int   m_ExitCode;                   // the exit code to be returned to the command line

        bool  m_DXUTInited;                 // if true, then DXUTInit() has succeeded
        bool  m_WindowCreated;              // if true, then DXUTCreateWindow() or DXUTSetWindow() has succeeded
        bool  m_DeviceCreated;              // if true, then DXUTCreateDevice() or DXUTSetD3D*Device() has succeeded

        bool  m_DXUTInitCalled;             // if true, then DXUTInit() was called
        bool  m_WindowCreateCalled;         // if true, then DXUTCreateWindow() or DXUTSetWindow() was called
        bool  m_DeviceCreateCalled;         // if true, then DXUTCreateDevice() or DXUTSetD3D*Device() was called

        bool  m_DeviceObjectsCreated;       // if true, then DeviceCreated callback has been called (if non-NULL)
        bool  m_DeviceObjectsReset;         // if true, then DeviceReset callback has been called (if non-NULL)
        bool  m_InsideDeviceCallback;       // if true, then the framework is inside an app device callback
        bool  m_InsideMainloop;             // if true, then the framework is inside the main loop
        bool  m_Active;                     // if true, then the app is the active top level window
        bool  m_TimePaused;                 // if true, then time is paused
        bool  m_RenderingPaused;            // if true, then rendering is paused
        int   m_PauseRenderingCount;        // pause rendering ref count
        int   m_PauseTimeCount;             // pause time ref count
        bool  m_DeviceLost;                 // if true, then the device is lost and needs to be reset
        bool  m_NotifyOnMouseMove;          // if true, include WM_MOUSEMOVE in mousecallback
        bool  m_Automation;                 // if true, automation is enabled
        bool  m_InSizeMove;                 // if true, app is inside a WM_ENTERSIZEMOVE
        UINT  m_TimerLastID;                // last ID of the DXUT timer
        bool  m_MessageWhenD3D11NotAvailable; 
        
        D3D_FEATURE_LEVEL  m_OverrideForceFeatureLevel; // if != -1, then overrid to use a featurelevel
        WCHAR m_ScreenShotName[256];        // command line screen shot name
        bool m_SaveScreenShot;              // command line save screen shot
        bool m_ExitAfterScreenShot;         // command line exit after screen shot
        
        int   m_OverrideForceAPI;               // if != -1, then override to use this Direct3D API version
        int   m_OverrideAdapterOrdinal;         // if != -1, then override to use this adapter ordinal
        bool  m_OverrideWindowed;               // if true, then force to start windowed
        int   m_OverrideOutput;                 // if != -1, then override to use the particular output on the adapter
        bool  m_OverrideFullScreen;             // if true, then force to start full screen
        int   m_OverrideStartX;                 // if != -1, then override to this X position of the window
        int   m_OverrideStartY;                 // if != -1, then override to this Y position of the window
        int   m_OverrideWidth;                  // if != 0, then override to this width
        int   m_OverrideHeight;                 // if != 0, then override to this height
        bool  m_OverrideForceHAL;               // if true, then force to HAL device (failing if one doesn't exist)
        bool  m_OverrideForceREF;               // if true, then force to REF device (failing if one doesn't exist)
        bool  m_OverrideConstantFrameTime;      // if true, then force to constant frame time
        float m_OverrideConstantTimePerFrame;   // the constant time per frame in seconds if m_OverrideConstantFrameTime==true
        int   m_OverrideQuitAfterFrame;         // if != 0, then it will force the app to quit after that frame
        int   m_OverrideForceVsync;             // if == 0, then it will force the app to use D3DPRESENT_INTERVAL_IMMEDIATE, if == 1 force use of D3DPRESENT_INTERVAL_DEFAULT
        bool  m_OverrideRelaunchMCE;            // if true, then force relaunch of MCE at exit
        bool  m_AppCalledWasKeyPressed;         // true if the app ever calls DXUTWasKeyPressed().  Allows for optimzation
        bool  m_ReleasingSwapChain;		        // if true, the app is releasing its swapchain

        LPDXUTCALLBACKMODIFYDEVICESETTINGS      m_ModifyDeviceSettingsFunc;     // modify Direct3D device settings callback
        LPDXUTCALLBACKDEVICEREMOVED             m_DeviceRemovedFunc;            // Direct3D device removed callback
        LPDXUTCALLBACKFRAMEMOVE                 m_FrameMoveFunc;                // frame move callback
        LPDXUTCALLBACKKEYBOARD                  m_KeyboardFunc;                 // keyboard callback
        LPDXUTCALLBACKMOUSE                     m_MouseFunc;                    // mouse callback
        LPDXUTCALLBACKMSGPROC                   m_WindowMsgFunc;                // window messages callback

        LPDXUTCALLBACKISD3D11DEVICEACCEPTABLE   m_IsD3D11DeviceAcceptableFunc;  // D3D11 is device acceptable callback
        LPDXUTCALLBACKD3D11DEVICECREATED        m_D3D11DeviceCreatedFunc;       // D3D11 device created callback
        LPDXUTCALLBACKD3D11SWAPCHAINRESIZED     m_D3D11SwapChainResizedFunc;    // D3D11 SwapChain reset callback
        LPDXUTCALLBACKD3D11SWAPCHAINRELEASING   m_D3D11SwapChainReleasingFunc;  // D3D11 SwapChain lost callback
        LPDXUTCALLBACKD3D11DEVICEDESTROYED      m_D3D11DeviceDestroyedFunc;     // D3D11 device destroyed callback
        LPDXUTCALLBACKD3D11FRAMERENDER          m_D3D11FrameRenderFunc;         // D3D11 frame render callback


        void* m_ModifyDeviceSettingsFuncUserContext;     // user context for modify Direct3D device settings callback
        void* m_DeviceRemovedFuncUserContext;            // user context for Direct3D device removed callback
        void* m_FrameMoveFuncUserContext;                // user context for frame move callback
        void* m_KeyboardFuncUserContext;                 // user context for keyboard callback
        void* m_MouseFuncUserContext;                    // user context for mouse callback
        void* m_WindowMsgFuncUserContext;                // user context for window messages callback

        void* m_IsD3D11DeviceAcceptableFuncUserContext;  // user context for is D3D11 device acceptable callback
        void* m_D3D11DeviceCreatedFuncUserContext;       // user context for D3D11 device created callback
        void* m_D3D11SwapChainResizedFuncUserContext;    // user context for D3D11 SwapChain resized callback
        void* m_D3D11SwapChainReleasingFuncUserContext;  // user context for D3D11 SwapChain releasing callback
        void* m_D3D11DeviceDestroyedFuncUserContext;     // user context for D3D11 device destroyed callback
        void* m_D3D11FrameRenderFuncUserContext;         // user context for D3D11 frame render callback

        bool m_Keys[256];                                // array of key state
        bool m_LastKeys[256];                            // array of last key state
        bool m_MouseButtons[5];                          // array of mouse states

        CGrowableArray<DXUT_TIMER>*  m_TimerList;        // list of DXUT_TIMER structs
        WCHAR m_StaticFrameStats[256];                   // static part of frames stats 
        WCHAR m_FPSStats[64];                            // fps stats
        WCHAR m_FrameStats[256];                         // frame stats (fps, width, etc)
        WCHAR m_DeviceStats[256];                        // device stats (description, device type, etc)
        WCHAR m_WindowTitle[256];                        // window title
   // };

    //STATE m_state;

public:
    DXUTState() 
	{
		Create();
	}
    ~DXUTState() { Destroy(); }

    void                                                                                    Create()
    {
        g_bThreadSafe = true;
        InitializeCriticalSectionAndSpinCount( &g_cs, 1000 );

        //ZeroMemory( &m_state, sizeof( STATE ) );
        m_OverrideStartX = -1;
        m_OverrideStartY = -1;
        m_OverrideForceFeatureLevel = (D3D_FEATURE_LEVEL)0;
        m_ScreenShotName[0] = 0;
        m_SaveScreenShot = false;
       m_ExitAfterScreenShot = false;
        m_OverrideForceAPI = -1;
        m_OverrideAdapterOrdinal = -1;
        m_OverrideOutput = -1;
       m_OverrideForceVsync = -1;
        m_AutoChangeAdapter = true;
        m_ShowMsgBoxOnError = true;
       m_AllowShortcutKeysWhenWindowed = true;
       m_Active = true;
      m_CallDefWindowProc = true;
     m_HandleEscape = true;
       m_HandleAltEnter = true;
      m_HandlePause = true;
		m_FPS = 1.0f;
        m_MessageWhenD3D11NotAvailable = true;
    }

    void Destroy()
    {
        SAFE_DELETE( m_TimerList );
        DXUTShutdown();
        DeleteCriticalSection( &g_cs );
    }

    // Macros to define access functions for thread safe access into m_state 
    GET_SET_ACCESSOR( DXUTDeviceSettings*, CurrentDeviceSettings );

    // D3D11 specific
    GET_SET_ACCESSOR( IDXGIFactory1*, DXGIFactory );
    GET_SET_ACCESSOR( IDXGIAdapter1*, DXGIAdapter );
    GET_SET_ACCESSOR( IDXGIOutput**, DXGIOutputArray );
    GET_SET_ACCESSOR( UINT, DXGIOutputArraySize );
    GET_SET_ACCESSOR( IDXGISwapChain*, DXGISwapChain );
    GETP_SETP_ACCESSOR( DXGI_SURFACE_DESC, BackBufferSurfaceDescDXGI );
    GET_SET_ACCESSOR( bool, RenderingOccluded );
    GET_SET_ACCESSOR( bool, DoNotStoreBufferSize );

    // D3D11 specific
    GET_SET_ACCESSOR( bool, D3D11Available );
    GET_SET_ACCESSOR( ID3D11Device*, D3D11Device );
    GET_SET_ACCESSOR( ID3D11DeviceContext*, D3D11DeviceContext );
    GET_SET_ACCESSOR( D3D_FEATURE_LEVEL, D3D11FeatureLevel );
    GET_SET_ACCESSOR( ID3D11Texture2D*, D3D11DepthStencil );
    GET_SET_ACCESSOR( ID3D11DepthStencilView*, D3D11DepthStencilView );   
    GET_SET_ACCESSOR( ID3D11RenderTargetView*, D3D11RenderTargetView );
    GET_SET_ACCESSOR( ID3D11RasterizerState*, D3D11RasterizerState );


    GET_SET_ACCESSOR( HWND, HWNDFocus );
    GET_SET_ACCESSOR( HWND, HWNDDeviceFullScreen );
    GET_SET_ACCESSOR( HWND, HWNDDeviceWindowed );
    GET_SET_ACCESSOR( HMONITOR, AdapterMonitor );
    GET_SET_ACCESSOR( HMENU, Menu );   


    GET_SET_ACCESSOR( UINT, FullScreenBackBufferWidthAtModeChange );
    GET_SET_ACCESSOR( UINT, FullScreenBackBufferHeightAtModeChange );
    GET_SET_ACCESSOR( UINT, WindowBackBufferWidthAtModeChange );
    GET_SET_ACCESSOR( UINT, WindowBackBufferHeightAtModeChange );
    GETP_SETP_ACCESSOR( WINDOWPLACEMENT, WindowedPlacement );
    GET_SET_ACCESSOR( DWORD, WindowedStyleAtModeChange );
    GET_SET_ACCESSOR( bool, TopmostWhileWindowed );
    GET_SET_ACCESSOR( bool, Minimized );
    GET_SET_ACCESSOR( bool, Maximized );
    GET_SET_ACCESSOR( bool, MinimizedWhileFullscreen );
    GET_SET_ACCESSOR( bool, IgnoreSizeChange );   

    GET_SET_ACCESSOR( double, Time );
    GET_SET_ACCESSOR( double, AbsoluteTime );
    GET_SET_ACCESSOR( float, ElapsedTime );

    GET_SET_ACCESSOR( HINSTANCE, HInstance );
    GET_SET_ACCESSOR( double, LastStatsUpdateTime );   
    GET_SET_ACCESSOR( DWORD, LastStatsUpdateFrames );   
    GET_SET_ACCESSOR( float, FPS );    
    GET_SET_ACCESSOR( int, CurrentFrameNumber );
    GET_SET_ACCESSOR( HHOOK, KeyboardHook );
    GET_SET_ACCESSOR( bool, AllowShortcutKeysWhenFullscreen );
    GET_SET_ACCESSOR( bool, AllowShortcutKeysWhenWindowed );
    GET_SET_ACCESSOR( bool, AllowShortcutKeys );
    GET_SET_ACCESSOR( bool, CallDefWindowProc );
    GET_SET_ACCESSOR( STICKYKEYS, StartupStickyKeys );
    GET_SET_ACCESSOR( TOGGLEKEYS, StartupToggleKeys );
    GET_SET_ACCESSOR( FILTERKEYS, StartupFilterKeys );

    GET_SET_ACCESSOR( bool, AppSupportsD3D9Override );
    GET_SET_ACCESSOR( bool, AppSupportsD3D11Override );
    GET_SET_ACCESSOR( bool, UseD3DVersionOverride );

    GET_SET_ACCESSOR( bool, HandleEscape );
    GET_SET_ACCESSOR( bool, HandleAltEnter );
    GET_SET_ACCESSOR( bool, HandlePause );
    GET_SET_ACCESSOR( bool, ShowMsgBoxOnError );
    GET_SET_ACCESSOR( bool, NoStats );
    GET_SET_ACCESSOR( bool, ClipCursorWhenFullScreen );   
    GET_SET_ACCESSOR( bool, ShowCursorWhenFullScreen );
    GET_SET_ACCESSOR( bool, ConstantFrameTime );
    GET_SET_ACCESSOR( float, TimePerFrame );
    GET_SET_ACCESSOR( bool, WireframeMode );   
    GET_SET_ACCESSOR( bool, AutoChangeAdapter );
    GET_SET_ACCESSOR( bool, WindowCreatedWithDefaultPositions );
    GET_SET_ACCESSOR( int, ExitCode );

    GET_SET_ACCESSOR( bool, DXUTInited );
    GET_SET_ACCESSOR( bool, WindowCreated );
    GET_SET_ACCESSOR( bool, DeviceCreated );
    GET_SET_ACCESSOR( bool, DXUTInitCalled );
    GET_SET_ACCESSOR( bool, WindowCreateCalled );
    GET_SET_ACCESSOR( bool, DeviceCreateCalled );
    GET_SET_ACCESSOR( bool, InsideDeviceCallback );
    GET_SET_ACCESSOR( bool, InsideMainloop );
    GET_SET_ACCESSOR( bool, DeviceObjectsCreated );
    GET_SET_ACCESSOR( bool, DeviceObjectsReset );
    GET_SET_ACCESSOR( bool, Active );
    GET_SET_ACCESSOR( bool, RenderingPaused );
    GET_SET_ACCESSOR( bool, TimePaused );
    GET_SET_ACCESSOR( int, PauseRenderingCount );
    GET_SET_ACCESSOR( int, PauseTimeCount );
    GET_SET_ACCESSOR( bool, DeviceLost );
    GET_SET_ACCESSOR( bool, NotifyOnMouseMove );
    GET_SET_ACCESSOR( bool, Automation );
    GET_SET_ACCESSOR( bool, InSizeMove );
    GET_SET_ACCESSOR( UINT, TimerLastID );
    GET_SET_ACCESSOR( bool, MessageWhenD3D11NotAvailable );
    GET_SET_ACCESSOR( bool, AppCalledWasKeyPressed );

    GET_SET_ACCESSOR( D3D_FEATURE_LEVEL, OverrideForceFeatureLevel );
    GET_ACCESSOR( WCHAR*, ScreenShotName );
    GET_SET_ACCESSOR( bool, SaveScreenShot );
    GET_SET_ACCESSOR( bool, ExitAfterScreenShot );

    
    GET_SET_ACCESSOR( int, OverrideForceAPI );
    GET_SET_ACCESSOR( int, OverrideAdapterOrdinal );
    GET_SET_ACCESSOR( bool, OverrideWindowed );
    GET_SET_ACCESSOR( int, OverrideOutput );
    GET_SET_ACCESSOR( bool, OverrideFullScreen );
    GET_SET_ACCESSOR( int, OverrideStartX );
    GET_SET_ACCESSOR( int, OverrideStartY );
    GET_SET_ACCESSOR( int, OverrideWidth );
    GET_SET_ACCESSOR( int, OverrideHeight );
    GET_SET_ACCESSOR( bool, OverrideForceHAL );
    GET_SET_ACCESSOR( bool, OverrideForceREF );
    GET_SET_ACCESSOR( bool, OverrideConstantFrameTime );
    GET_SET_ACCESSOR( float, OverrideConstantTimePerFrame );
    GET_SET_ACCESSOR( int, OverrideQuitAfterFrame );
    GET_SET_ACCESSOR( int, OverrideForceVsync );
    GET_SET_ACCESSOR( bool, OverrideRelaunchMCE );
    GET_SET_ACCESSOR( bool, ReleasingSwapChain );
    
    GET_SET_ACCESSOR( LPDXUTCALLBACKMODIFYDEVICESETTINGS, ModifyDeviceSettingsFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKDEVICEREMOVED, DeviceRemovedFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKFRAMEMOVE, FrameMoveFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKKEYBOARD, KeyboardFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKMOUSE, MouseFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKMSGPROC, WindowMsgFunc );

    GET_SET_ACCESSOR( LPDXUTCALLBACKISD3D11DEVICEACCEPTABLE, IsD3D11DeviceAcceptableFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D11DEVICECREATED, D3D11DeviceCreatedFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D11SWAPCHAINRESIZED, D3D11SwapChainResizedFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D11SWAPCHAINRELEASING, D3D11SwapChainReleasingFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D11DEVICEDESTROYED, D3D11DeviceDestroyedFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D11FRAMERENDER, D3D11FrameRenderFunc );

    GET_SET_ACCESSOR( void*, ModifyDeviceSettingsFuncUserContext );
    GET_SET_ACCESSOR( void*, DeviceRemovedFuncUserContext );
    GET_SET_ACCESSOR( void*, FrameMoveFuncUserContext );
    GET_SET_ACCESSOR( void*, KeyboardFuncUserContext );
    GET_SET_ACCESSOR( void*, MouseFuncUserContext );
    GET_SET_ACCESSOR( void*, WindowMsgFuncUserContext );

    GET_SET_ACCESSOR( void*, IsD3D11DeviceAcceptableFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D11DeviceCreatedFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D11DeviceDestroyedFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D11SwapChainResizedFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D11SwapChainReleasingFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D11FrameRenderFuncUserContext );

    GET_SET_ACCESSOR( CGrowableArray<DXUT_TIMER>*, TimerList );
    GET_ACCESSOR( bool*, Keys );
    GET_ACCESSOR( bool*, LastKeys );
    GET_ACCESSOR( bool*, MouseButtons );
    GET_ACCESSOR( WCHAR*, StaticFrameStats );
    GET_ACCESSOR( WCHAR*, FPSStats );
    GET_ACCESSOR( WCHAR*, FrameStats );
    GET_ACCESSOR( WCHAR*, DeviceStats );    
    GET_ACCESSOR( WCHAR*, WindowTitle );
};


//--------------------------------------------------------------------------------------
// Global state 
//--------------------------------------------------------------------------------------
DXUTState g_pDXUTState;

class DXUTMemoryHelper
{
public:
    DXUTMemoryHelper()  { DXUTCreateState(); }
    ~DXUTMemoryHelper() { DXUTDestroyState(); }
};

//--------------------------------------------------------------------------------------
// Internal functions forward declarations
//--------------------------------------------------------------------------------------
void DXUTParseCommandLine( __inout WCHAR* strCommandLine, 
                           bool bIgnoreFirstCommand = true );
bool DXUTIsNextArg( __inout WCHAR*& strCmdLine, __in const WCHAR* strArg );
bool DXUTGetCmdParam( __inout WCHAR*& strCmdLine, __out WCHAR* strFlag );
void DXUTUpdateFrameStats();

LRESULT CALLBACK DXUTStaticWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
void DXUTHandleTimers();
void DXUTDisplayErrorMessage( HRESULT hr );
int DXUTMapButtonToArrayIndex( BYTE vButton );

HRESULT DXUTChangeDevice( DXUTDeviceSettings* pNewDeviceSettings,
                          IDirect3DDevice9* pd3d9DeviceFromApp,
                          ID3D11Device* pd3d11DeviceFromApp,
                          bool bForceRecreate,
                          bool bClipWindowToSingleAdapter );

bool DXUTCanDeviceBeReset( DXUTDeviceSettings* pOldDeviceSettings,
                           DXUTDeviceSettings* pNewDeviceSettings,
                           IDirect3DDevice9* pd3d9DeviceFromApp,
                           ID3D11Device* pd3d11DeviceFromApp );


HRESULT DXUTDelayLoadDXGI();
HRESULT DXUTSnapDeviceSettingsToEnumDevice( DXUTDeviceSettings* pDeviceSettings, bool forceEnum, D3D_FEATURE_LEVEL forceFL = D3D_FEATURE_LEVEL(0) );
void DXUTUpdateDeviceSettingsWithOverrides( DXUTDeviceSettings* pDeviceSettings );
void DXUTCheckForDXGIFullScreenSwitch();
void DXUTResizeDXGIBuffers( UINT Width, UINT Height, BOOL bFullscreen );
void DXUTCheckForDXGIBufferChange();
void DXUTCheckForWindowSizeChange();
HMONITOR DXUTGetMonitorFromAdapter();
HRESULT DXUTGetAdapterOrdinalFromMonitor( HMONITOR hMonitor, UINT* pAdapterOrdinal );
HRESULT DXUTGetOutputOrdinalFromMonitor( HMONITOR hMonitor, UINT* pOutputOrdinal );
HRESULT DXUTHandleDeviceRemoved();
void DXUTUpdateBackBufferDesc();
void DXUTSetupCursor();

// Direct3D 11
HRESULT DXUTCreateD3D11Views( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dDeviceContext, DXUTDeviceSettings* pDeviceSettings );
HRESULT DXUTCreate3DEnvironment11( ID3D11Device* pd3dDeviceFromApp );
HRESULT DXUTReset3DEnvironment11();
void DXUTRender3DEnvironment11();
void DXUTUpdateD3D11DeviceStats( D3D_DRIVER_TYPE DeviceType, DXGI_ADAPTER_DESC* pAdapterDesc );

bool DXUTGetIsWindowedFromDS( DXUTDeviceSettings* pNewDeviceSettings )          
{ 
    if (!pNewDeviceSettings) 
        return true; 
    
    return pNewDeviceSettings->d3d11.sd.Windowed ? true : false; 
}


//--------------------------------------------------------------------------------------
// External state access functions
//--------------------------------------------------------------------------------------
BOOL WINAPI DXUTGetMSAASwapChainCreated() { 
	DXUTDeviceSettings *psettings = g_pDXUTState.GetCurrentDeviceSettings();
    if (psettings->ver == DXUT_D3D11_DEVICE) {
        return psettings->d3d11.sd.SampleDesc.Count > 1;
    }
    else return false;
}
ID3D11Device* WINAPI DXUTGetD3D11Device()                  { return g_pDXUTState.GetD3D11Device(); }
D3D_FEATURE_LEVEL	 WINAPI DXUTGetD3D11DeviceFeatureLevel() { return g_pDXUTState.GetD3D11FeatureLevel(); }
ID3D11DeviceContext* WINAPI DXUTGetD3D11DeviceContext()    { return g_pDXUTState.GetD3D11DeviceContext(); }
IDXGISwapChain* WINAPI DXUTGetDXGISwapChain()              { return g_pDXUTState.GetDXGISwapChain(); }
ID3D11RenderTargetView* WINAPI DXUTGetD3D11RenderTargetView() { return g_pDXUTState.GetD3D11RenderTargetView(); }
ID3D11DepthStencilView* WINAPI DXUTGetD3D11DepthStencilView() { return g_pDXUTState.GetD3D11DepthStencilView(); }
const DXGI_SURFACE_DESC* WINAPI DXUTGetDXGIBackBufferSurfaceDesc() { return g_pDXUTState.GetBackBufferSurfaceDescDXGI(); }
HINSTANCE WINAPI DXUTGetHINSTANCE()                        { return g_pDXUTState.GetHInstance(); }
HWND WINAPI DXUTGetHWND()                                  { return DXUTIsWindowed() ? g_pDXUTState.GetHWNDDeviceWindowed() : g_pDXUTState.GetHWNDDeviceFullScreen(); }
HWND WINAPI DXUTGetHWNDFocus()                             { return g_pDXUTState.GetHWNDFocus(); }
HWND WINAPI DXUTGetHWNDDeviceFullScreen()                  { return g_pDXUTState.GetHWNDDeviceFullScreen(); }
HWND WINAPI DXUTGetHWNDDeviceWindowed()                    { return g_pDXUTState.GetHWNDDeviceWindowed(); }
RECT WINAPI DXUTGetWindowClientRect()                      { RECT rc; GetClientRect( DXUTGetHWND(), &rc ); return rc; }
LONG WINAPI DXUTGetWindowWidth()                           { RECT rc = DXUTGetWindowClientRect(); return ((LONG)rc.right - rc.left); }
LONG WINAPI DXUTGetWindowHeight()                          { RECT rc = DXUTGetWindowClientRect(); return ((LONG)rc.bottom - rc.top); }
RECT WINAPI DXUTGetWindowClientRectAtModeChange()          { RECT rc = { 0, 0, g_pDXUTState.GetWindowBackBufferWidthAtModeChange(), g_pDXUTState.GetWindowBackBufferHeightAtModeChange() }; return rc; }
RECT WINAPI DXUTGetFullsceenClientRectAtModeChange()       { RECT rc = { 0, 0, g_pDXUTState.GetFullScreenBackBufferWidthAtModeChange(), g_pDXUTState.GetFullScreenBackBufferHeightAtModeChange() }; return rc; }
double WINAPI DXUTGetTime()                                { return g_pDXUTState.GetTime(); }
float WINAPI DXUTGetElapsedTime()                          { return g_pDXUTState.GetElapsedTime(); }
float WINAPI DXUTGetFPS()                                  { return g_pDXUTState.GetFPS(); }
LPCWSTR WINAPI DXUTGetWindowTitle()                        { return g_pDXUTState.GetWindowTitle(); }
LPCWSTR WINAPI DXUTGetDeviceStats()                        { return g_pDXUTState.GetDeviceStats(); }
bool WINAPI DXUTIsRenderingPaused()                        { return g_pDXUTState.GetPauseRenderingCount() > 0; }
bool WINAPI DXUTIsTimePaused()                             { return g_pDXUTState.GetPauseTimeCount() > 0; }
bool WINAPI DXUTIsActive()                                 { return g_pDXUTState.GetActive(); }
int WINAPI DXUTGetExitCode()                               { return g_pDXUTState.GetExitCode(); }
bool WINAPI DXUTGetShowMsgBoxOnError()                     { return g_pDXUTState.GetShowMsgBoxOnError(); }
bool WINAPI DXUTGetAutomation()                            { return g_pDXUTState.GetAutomation(); }
bool WINAPI DXUTIsWindowed()                               { return DXUTGetIsWindowedFromDS( g_pDXUTState.GetCurrentDeviceSettings() ); }
IDXGIFactory1* WINAPI DXUTGetDXGIFactory()                  { DXUTDelayLoadDXGI(); return g_pDXUTState.GetDXGIFactory(); }
bool WINAPI DXUTIsD3D11Available()                         { DXUTDelayLoadDXGI(); return g_pDXUTState.GetD3D11Available(); }
bool WINAPI DXUTIsAppRenderingWithD3D11()                  { return (g_pDXUTState.GetD3D11Device() != NULL); }

//--------------------------------------------------------------------------------------
// External callback setup functions
//--------------------------------------------------------------------------------------

// General callbacks
void WINAPI DXUTSetCallbackDeviceChanging( LPDXUTCALLBACKMODIFYDEVICESETTINGS pCallback, void* pUserContext )                  { g_pDXUTState.SetModifyDeviceSettingsFunc( pCallback ); g_pDXUTState.SetModifyDeviceSettingsFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackDeviceRemoved( LPDXUTCALLBACKDEVICEREMOVED pCallback, void* pUserContext )                          { g_pDXUTState.SetDeviceRemovedFunc( pCallback ); g_pDXUTState.SetDeviceRemovedFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackFrameMove( LPDXUTCALLBACKFRAMEMOVE pCallback, void* pUserContext )                                  { g_pDXUTState.SetFrameMoveFunc( pCallback );  g_pDXUTState.SetFrameMoveFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackKeyboard( LPDXUTCALLBACKKEYBOARD pCallback, void* pUserContext )                                    { g_pDXUTState.SetKeyboardFunc( pCallback );  g_pDXUTState.SetKeyboardFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackMouse( LPDXUTCALLBACKMOUSE pCallback, bool bIncludeMouseMove, void* pUserContext )                  { g_pDXUTState.SetMouseFunc( pCallback ); g_pDXUTState.SetNotifyOnMouseMove( bIncludeMouseMove );  g_pDXUTState.SetMouseFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackMsgProc( LPDXUTCALLBACKMSGPROC pCallback, void* pUserContext )                                      { g_pDXUTState.SetWindowMsgFunc( pCallback );  g_pDXUTState.SetWindowMsgFuncUserContext( pUserContext ); }

// Direct3D 11 callbacks
void WINAPI DXUTSetCallbackD3D11DeviceAcceptable( LPDXUTCALLBACKISD3D11DEVICEACCEPTABLE pCallback, void* pUserContext )        { g_pDXUTState.SetIsD3D11DeviceAcceptableFunc( pCallback ); g_pDXUTState.SetIsD3D11DeviceAcceptableFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D11DeviceCreated( LPDXUTCALLBACKD3D11DEVICECREATED pCallback, void* pUserContext )                { g_pDXUTState.SetD3D11DeviceCreatedFunc( pCallback ); g_pDXUTState.SetD3D11DeviceCreatedFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D11SwapChainResized( LPDXUTCALLBACKD3D11SWAPCHAINRESIZED pCallback, void* pUserContext )          { g_pDXUTState.SetD3D11SwapChainResizedFunc( pCallback );  g_pDXUTState.SetD3D11SwapChainResizedFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D11FrameRender( LPDXUTCALLBACKD3D11FRAMERENDER pCallback, void* pUserContext )                    { g_pDXUTState.SetD3D11FrameRenderFunc( pCallback );  g_pDXUTState.SetD3D11FrameRenderFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D11SwapChainReleasing( LPDXUTCALLBACKD3D11SWAPCHAINRELEASING pCallback, void* pUserContext )      { g_pDXUTState.SetD3D11SwapChainReleasingFunc( pCallback );  g_pDXUTState.SetD3D11SwapChainReleasingFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D11DeviceDestroyed( LPDXUTCALLBACKD3D11DEVICEDESTROYED pCallback, void* pUserContext )            { g_pDXUTState.SetD3D11DeviceDestroyedFunc( pCallback );  g_pDXUTState.SetD3D11DeviceDestroyedFuncUserContext( pUserContext ); }
void DXUTGetCallbackD3D11DeviceAcceptable( LPDXUTCALLBACKISD3D11DEVICEACCEPTABLE* ppCallback, void** ppUserContext )           { *ppCallback = g_pDXUTState.GetIsD3D11DeviceAcceptableFunc(); *ppUserContext = g_pDXUTState.GetIsD3D11DeviceAcceptableFuncUserContext(); }


//--------------------------------------------------------------------------------------
// Optionally parses the command line and sets if default hotkeys are handled
//
//       Possible command line parameters are:
//          -forcefeaturelevel:fl     forces app to use a specified direct3D11 feature level    
//          -screenshotexit:filename save a screenshot to the filename.bmp and exit.
//          -forceapi:#             forces app to use specified Direct3D API version (fails if the application doesn't support this API or if no device is found)
//          -adapter:#              forces app to use this adapter # (fails if the adapter doesn't exist)
//          -output:#               [D3D11 only] forces app to use a particular output on the adapter (fails if the output doesn't exist) 
//          -windowed               forces app to start windowed
//          -fullscreen             forces app to start full screen
//          -forcehal               forces app to use HAL (fails if HAL doesn't exist)
//          -forceref               forces app to use REF (fails if REF doesn't exist)
//          -forcepurehwvp          [D3D9 only] forces app to use pure HWVP (fails if device doesn't support it)
//          -forcehwvp              [D3D9 only] forces app to use HWVP (fails if device doesn't support it)
//          -forceswvp              [D3D9 only] forces app to use SWVP 
//          -forcevsync:#           if # is 0, then vsync is disabled 
//          -width:#                forces app to use # for width. for full screen, it will pick the closest possible supported mode
//          -height:#               forces app to use # for height. for full screen, it will pick the closest possible supported mode
//          -startx:#               forces app to use # for the x coord of the window position for windowed mode
//          -starty:#               forces app to use # for the y coord of the window position for windowed mode
//          -constantframetime:#    forces app to use constant frame time, where # is the time/frame in seconds
//          -quitafterframe:x       forces app to quit after # frames
//          -noerrormsgboxes        prevents the display of message boxes generated by the framework so the application can be run without user interaction
//          -nostats                prevents the display of the stats
//          -relaunchmce            re-launches the MCE UI after the app exits
//          -automation             a hint to other components that automation is active 
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTInit( bool bParseCommandLine, 
                         bool bShowMsgBoxOnError, 
                         __in_opt WCHAR* strExtraCommandLineParams, 
                         bool bThreadSafeDXUT )
{
    g_bThreadSafe = bThreadSafeDXUT;

    g_pDXUTState.SetDXUTInitCalled( true );

    // Not always needed, but lets the app create GDI dialogs
    InitCommonControls();

    // Save the current sticky/toggle/filter key settings so DXUT can restore them later
    STICKYKEYS sk = {sizeof(STICKYKEYS), 0};
    SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &sk, 0);
    g_pDXUTState.SetStartupStickyKeys( sk );

    TOGGLEKEYS tk = {sizeof(TOGGLEKEYS), 0};
    SystemParametersInfo(SPI_GETTOGGLEKEYS, sizeof(TOGGLEKEYS), &tk, 0);
    g_pDXUTState.SetStartupToggleKeys( tk );

    FILTERKEYS fk = {sizeof(FILTERKEYS), 0};
    SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &fk, 0);
    g_pDXUTState.SetStartupFilterKeys( fk );

    g_pDXUTState.SetShowMsgBoxOnError( bShowMsgBoxOnError );

    if( bParseCommandLine )
        DXUTParseCommandLine( GetCommandLine() );
    if( strExtraCommandLineParams )
        DXUTParseCommandLine( strExtraCommandLineParams, false );

    // Reset the timer
    g_pDXUTState.SetDXUTInited( true );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Parses the command line for parameters.  See DXUTInit() for list 
//--------------------------------------------------------------------------------------
void DXUTParseCommandLine(__inout WCHAR* strCommandLine, 
                          bool bIgnoreFirstCommand  )
{
    WCHAR* strCmdLine;
    WCHAR strFlag[MAX_PATH];

    int nNumArgs;
    LPWSTR* pstrArgList = CommandLineToArgvW( strCommandLine, &nNumArgs );
	int iArgStart = 0;
	if( bIgnoreFirstCommand )
        iArgStart = 1;
    for( int iArg = iArgStart; iArg < nNumArgs; iArg++ )
    {
        strCmdLine = pstrArgList[iArg];

        // Handle flag args
        if( *strCmdLine == L'/' || *strCmdLine == L'-' )
        {
            strCmdLine++;

            if( DXUTIsNextArg( strCmdLine, L"forcefeaturelevel" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    if (_wcsnicmp( strFlag, L"D3D_FEATURE_LEVEL_11_0", MAX_PATH) == 0 ) {
                        g_pDXUTState.SetOverrideForceFeatureLevel(D3D_FEATURE_LEVEL_11_0);
                    }else if (_wcsnicmp( strFlag, L"D3D_FEATURE_LEVEL_10_1", MAX_PATH) == 0 ) {
                        g_pDXUTState.SetOverrideForceFeatureLevel(D3D_FEATURE_LEVEL_10_1);
                    }else if (_wcsnicmp( strFlag, L"D3D_FEATURE_LEVEL_10_0", MAX_PATH) == 0 ) {
                        g_pDXUTState.SetOverrideForceFeatureLevel(D3D_FEATURE_LEVEL_10_0);
                    }else if (_wcsnicmp( strFlag, L"D3D_FEATURE_LEVEL_9_3", MAX_PATH) == 0 ) {
                        g_pDXUTState.SetOverrideForceFeatureLevel(D3D_FEATURE_LEVEL_9_3);
                    }else if (_wcsnicmp( strFlag, L"D3D_FEATURE_LEVEL_9_2", MAX_PATH) == 0 ) {
                        g_pDXUTState.SetOverrideForceFeatureLevel(D3D_FEATURE_LEVEL_9_2);
                    }else if (_wcsnicmp( strFlag, L"D3D_FEATURE_LEVEL_9_1", MAX_PATH) == 0 ) {
                        g_pDXUTState.SetOverrideForceFeatureLevel(D3D_FEATURE_LEVEL_9_1);
                    }


                    continue;
                }
            }
            
            if( DXUTIsNextArg( strCmdLine, L"forceapi" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nAPIVersion = _wtoi( strFlag );
                    g_pDXUTState.SetOverrideForceAPI( nAPIVersion );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"adapter" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nAdapter = _wtoi( strFlag );
                    g_pDXUTState.SetOverrideAdapterOrdinal( nAdapter );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"windowed" ) )
            {
                g_pDXUTState.SetOverrideWindowed( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"output" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int Output = _wtoi( strFlag );
                    g_pDXUTState.SetOverrideOutput( Output );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"fullscreen" ) )
            {
                g_pDXUTState.SetOverrideFullScreen( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"forcehal" ) )
            {
                g_pDXUTState.SetOverrideForceHAL( true );
                continue;
            }
            if( DXUTIsNextArg( strCmdLine, L"screenshotexit" ) ) {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    g_pDXUTState.SetExitAfterScreenShot( true );
                    g_pDXUTState.SetSaveScreenShot( true );
                    swprintf_s( g_pDXUTState.GetScreenShotName(), 256, L"%s.bmp", strFlag );
                    continue;
                }
            }
            if( DXUTIsNextArg( strCmdLine, L"forceref" ) )
            {
                g_pDXUTState.SetOverrideForceREF( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"forcevsync" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nOn = _wtoi( strFlag );
                    g_pDXUTState.SetOverrideForceVsync( nOn );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"width" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nWidth = _wtoi( strFlag );
                    g_pDXUTState.SetOverrideWidth( nWidth );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"height" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nHeight = _wtoi( strFlag );
                    g_pDXUTState.SetOverrideHeight( nHeight );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"startx" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nX = _wtoi( strFlag );
                    g_pDXUTState.SetOverrideStartX( nX );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"starty" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nY = _wtoi( strFlag );
                    g_pDXUTState.SetOverrideStartY( nY );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"constantframetime" ) )
            {
                float fTimePerFrame;
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                    fTimePerFrame = ( float )wcstod( strFlag, NULL );
                else
                    fTimePerFrame = 0.0333f;
                g_pDXUTState.SetOverrideConstantFrameTime( true );
                g_pDXUTState.SetOverrideConstantTimePerFrame( fTimePerFrame );
                DXUTSetConstantFrameTime( true, fTimePerFrame );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"quitafterframe" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nFrame = _wtoi( strFlag );
                    g_pDXUTState.SetOverrideQuitAfterFrame( nFrame );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"noerrormsgboxes" ) )
            {
                g_pDXUTState.SetShowMsgBoxOnError( false );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"nostats" ) )
            {
                g_pDXUTState.SetNoStats( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"relaunchmce" ) )
            {
                g_pDXUTState.SetOverrideRelaunchMCE( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"automation" ) )
            {
                g_pDXUTState.SetAutomation( true );
                continue;
            }
        }

        // Unrecognized flag
        wcscpy_s( strFlag, MAX_PATH, strCmdLine );
        WCHAR* strSpace = strFlag;
        while( *strSpace && ( *strSpace > L' ' ) )
            strSpace++;
        *strSpace = 0;

        strCmdLine += wcslen( strFlag );
    }

    LocalFree( pstrArgList );
}


//--------------------------------------------------------------------------------------
// Helper function for DXUTParseCommandLine
//--------------------------------------------------------------------------------------
bool DXUTIsNextArg( WCHAR*& strCmdLine, const WCHAR* strArg )
{
    int nArgLen = ( int )wcslen( strArg );
    int nCmdLen = ( int )wcslen( strCmdLine );

    if( nCmdLen >= nArgLen &&
        _wcsnicmp( strCmdLine, strArg, nArgLen ) == 0 &&
        ( strCmdLine[nArgLen] == 0 || strCmdLine[nArgLen] == L':' ) )
    {
        strCmdLine += nArgLen;
        return true;
    }

    return false;
}


//--------------------------------------------------------------------------------------
// Helper function for DXUTParseCommandLine.  Updates strCmdLine and strFlag 
//      Example: if strCmdLine=="-width:1024 -forceref"
// then after: strCmdLine==" -forceref" and strFlag=="1024"
//--------------------------------------------------------------------------------------
bool DXUTGetCmdParam( WCHAR*& strCmdLine, WCHAR* strFlag )
{
    if( *strCmdLine == L':' )
    {
        strCmdLine++; // Skip ':'

        // Place NULL terminator in strFlag after current token
        wcscpy_s( strFlag, MAX_PATH, strCmdLine );
        WCHAR* strSpace = strFlag;
        while( *strSpace && ( *strSpace > L' ' ) )
            strSpace++;
        *strSpace = 0;

        // Update strCmdLine
        strCmdLine += wcslen( strFlag );
        return true;
    }
    else
    {
        strFlag[0] = 0;
        return false;
    }
}


//--------------------------------------------------------------------------------------
// Creates a window with the specified window title, icon, menu, and 
// starting position.  If DXUTInit() has not already been called, it will
// call it with the default parameters.  Instead of calling this, you can 
// call DXUTSetWindow() to use an existing window.  
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTCreateWindow( const WCHAR* strWindowTitle, HINSTANCE hInstance,
                                 HICON hIcon, HMENU hMenu, int x, int y )
{
    HRESULT hr;

    // Not allowed to call this from inside the device callbacks
    if( g_pDXUTState.GetInsideDeviceCallback() )
        return E_FAIL;

    g_pDXUTState.SetWindowCreateCalled( true );

    if( !g_pDXUTState.GetDXUTInited() )
    {
        // If DXUTInit() was already called and failed, then fail.
        // DXUTInit() must first succeed for this function to succeed
        if( g_pDXUTState.GetDXUTInitCalled() )
            return E_FAIL;

        // If DXUTInit() hasn't been called, then automatically call it
        // with default params
        hr = DXUTInit();
        if( FAILED( hr ) )
            return hr;
    }

    if( DXUTGetHWNDFocus() == NULL )
    {
        if( hInstance == NULL )
            hInstance = ( HINSTANCE )GetModuleHandle( NULL );
        g_pDXUTState.SetHInstance( hInstance );

        WCHAR szExePath[MAX_PATH];
        GetModuleFileName( NULL, szExePath, MAX_PATH );
        if( hIcon == NULL ) // If the icon is NULL, then use the first one found in the exe
            hIcon = ExtractIcon( hInstance, szExePath, 0 );

        // Register the windows class
        WNDCLASS wndClass;
        wndClass.style = CS_DBLCLKS;
        wndClass.lpfnWndProc = DXUTStaticWndProc;
        wndClass.cbClsExtra = 0;
        wndClass.cbWndExtra = 0;
        wndClass.hInstance = hInstance;
        wndClass.hIcon = hIcon;
        wndClass.hCursor = LoadCursor( NULL, IDC_ARROW );
        wndClass.hbrBackground = ( HBRUSH )GetStockObject( BLACK_BRUSH );
        wndClass.lpszMenuName = NULL;
        wndClass.lpszClassName = L"Direct3DWindowClass";

        if( !RegisterClass( &wndClass ) )
        {
            DWORD dwError = GetLastError();
            if( dwError != ERROR_CLASS_ALREADY_EXISTS )
                return HRESULT_FROM_WIN32(dwError) ;
        }

        // Override the window's initial & size position if there were cmd line args
        if( g_pDXUTState.GetOverrideStartX() != -1 )
            x = g_pDXUTState.GetOverrideStartX();
        if( g_pDXUTState.GetOverrideStartY() != -1 )
            y = g_pDXUTState.GetOverrideStartY();

        g_pDXUTState.SetWindowCreatedWithDefaultPositions( false );
        if( x == CW_USEDEFAULT && y == CW_USEDEFAULT )
            g_pDXUTState.SetWindowCreatedWithDefaultPositions( true );

        // Find the window's initial size, but it might be changed later
        int nDefaultWidth = 640;
        int nDefaultHeight = 480;

        RECT rc;
        SetRect( &rc, 0, 0, nDefaultWidth, nDefaultHeight );
        AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, ( hMenu != NULL ) ? true : false );

        WCHAR* strCachedWindowTitle = g_pDXUTState.GetWindowTitle();
        wcscpy_s( strCachedWindowTitle, 256, strWindowTitle );

        // Create the render window
        HWND hWnd = CreateWindow( L"Direct3DWindowClass", strWindowTitle, WS_OVERLAPPEDWINDOW,
                                  x, y, ( rc.right - rc.left ), ( rc.bottom - rc.top ), 0,
                                  hMenu, hInstance, 0 );
        if( hWnd == NULL )
        {
            DWORD dwError = GetLastError();
            return HRESULT_FROM_WIN32(dwError);
        }

        g_pDXUTState.SetWindowCreated( true );
        g_pDXUTState.SetHWNDFocus( hWnd );
        g_pDXUTState.SetHWNDDeviceFullScreen( hWnd );
        g_pDXUTState.SetHWNDDeviceWindowed( hWnd );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Sets a previously created window for the framework to use.  If DXUTInit() 
// has not already been called, it will call it with the default parameters.  
// Instead of calling this, you can call DXUTCreateWindow() to create a new window.  
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTSetWindow( HWND hWndFocus, HWND hWndDeviceFullScreen, HWND hWndDeviceWindowed, bool bHandleMessages )
{
    HRESULT hr;

    // Not allowed to call this from inside the device callbacks
	if (g_pDXUTState.GetInsideDeviceCallback())
		return E_FAIL;

    g_pDXUTState.SetWindowCreateCalled( true );

    // To avoid confusion, we do not allow any HWND to be NULL here.  The
    // caller must pass in valid HWND for all three parameters.  The same
    // HWND may be used for more than one parameter.
    if( hWndFocus == NULL || hWndDeviceFullScreen == NULL || hWndDeviceWindowed == NULL )
        return E_INVALIDARG;

    // If subclassing the window, set the pointer to the local window procedure
    if( bHandleMessages )
    {
        // Switch window procedures
        LONG_PTR nResult = SetWindowLongPtr( hWndFocus, GWLP_WNDPROC, (LONG_PTR)DXUTStaticWndProc );

        DWORD dwError = GetLastError();
        if( nResult == 0 )
            return HRESULT_FROM_WIN32(dwError);
    }

    if( !g_pDXUTState.GetDXUTInited() )
    {
        // If DXUTInit() was already called and failed, then fail.
        // DXUTInit() must first succeed for this function to succeed
        if( g_pDXUTState.GetDXUTInitCalled() )
            return E_FAIL;

        // If DXUTInit() hasn't been called, then automatically call it
        // with default params
        hr = DXUTInit();
        if( FAILED( hr ) )
            return hr;
    }

    WCHAR* strCachedWindowTitle = g_pDXUTState.GetWindowTitle();
    GetWindowText( hWndFocus, strCachedWindowTitle, 255 );
    strCachedWindowTitle[255] = 0;

    HINSTANCE hInstance = ( HINSTANCE )( LONG_PTR )GetWindowLongPtr( hWndFocus, GWLP_HINSTANCE );
    g_pDXUTState.SetHInstance( hInstance );
    g_pDXUTState.SetWindowCreatedWithDefaultPositions( false );
    g_pDXUTState.SetWindowCreated( true );
    g_pDXUTState.SetHWNDFocus( hWndFocus );
    g_pDXUTState.SetHWNDDeviceFullScreen( hWndDeviceFullScreen );
    g_pDXUTState.SetHWNDDeviceWindowed( hWndDeviceWindowed );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handles window messages 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK DXUTStaticWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    
    // Consolidate the keyboard messages and pass them to the app's keyboard callback
    if( uMsg == WM_KEYDOWN ||
        uMsg == WM_SYSKEYDOWN ||
        uMsg == WM_KEYUP ||
        uMsg == WM_SYSKEYUP )
    {
        bool bKeyDown = ( uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN );
        DWORD dwMask = ( 1 << 29 );
        bool bAltDown = ( ( lParam & dwMask ) != 0 );

        bool* bKeys = g_pDXUTState.GetKeys();
        bKeys[ ( BYTE )( wParam & 0xFF ) ] = bKeyDown;

        LPDXUTCALLBACKKEYBOARD pCallbackKeyboard = g_pDXUTState.GetKeyboardFunc();
        if( pCallbackKeyboard )
            pCallbackKeyboard( ( UINT )wParam, bKeyDown, bAltDown, g_pDXUTState.GetKeyboardFuncUserContext() );
    }

    // Consolidate the mouse button messages and pass them to the app's mouse callback
    if( uMsg == WM_LBUTTONDOWN ||
        uMsg == WM_LBUTTONUP ||
        uMsg == WM_LBUTTONDBLCLK ||
        uMsg == WM_MBUTTONDOWN ||
        uMsg == WM_MBUTTONUP ||
        uMsg == WM_MBUTTONDBLCLK ||
        uMsg == WM_RBUTTONDOWN ||
        uMsg == WM_RBUTTONUP ||
        uMsg == WM_RBUTTONDBLCLK ||
        uMsg == WM_XBUTTONDOWN ||
        uMsg == WM_XBUTTONUP ||
        uMsg == WM_XBUTTONDBLCLK ||
        uMsg == WM_MOUSEWHEEL ||
        ( g_pDXUTState.GetNotifyOnMouseMove() && uMsg == WM_MOUSEMOVE ) )
    {
        int xPos = ( short )LOWORD( lParam );
        int yPos = ( short )HIWORD( lParam );

        if( uMsg == WM_MOUSEWHEEL )
        {
            // WM_MOUSEWHEEL passes screen mouse coords
            // so convert them to client coords
            POINT pt;
            pt.x = xPos; pt.y = yPos;
            ScreenToClient( hWnd, &pt );
            xPos = pt.x; yPos = pt.y;
        }

        int nMouseWheelDelta = 0;
        if( uMsg == WM_MOUSEWHEEL )
            nMouseWheelDelta = ( short )HIWORD( wParam );

        int nMouseButtonState = LOWORD( wParam );
        bool bLeftButton = ( ( nMouseButtonState & MK_LBUTTON ) != 0 );
        bool bRightButton = ( ( nMouseButtonState & MK_RBUTTON ) != 0 );
        bool bMiddleButton = ( ( nMouseButtonState & MK_MBUTTON ) != 0 );
        bool bSideButton1 = ( ( nMouseButtonState & MK_XBUTTON1 ) != 0 );
        bool bSideButton2 = ( ( nMouseButtonState & MK_XBUTTON2 ) != 0 );

        bool* bMouseButtons = g_pDXUTState.GetMouseButtons();
        bMouseButtons[0] = bLeftButton;
        bMouseButtons[1] = bMiddleButton;
        bMouseButtons[2] = bRightButton;
        bMouseButtons[3] = bSideButton1;
        bMouseButtons[4] = bSideButton2;

        LPDXUTCALLBACKMOUSE pCallbackMouse = g_pDXUTState.GetMouseFunc();
        if( pCallbackMouse )
            pCallbackMouse( bLeftButton, bRightButton, bMiddleButton, bSideButton1, bSideButton2, nMouseWheelDelta,
                            xPos, yPos, g_pDXUTState.GetMouseFuncUserContext() );
    }

    // Pass all messages to the app's MsgProc callback, and don't 
    // process further messages if the apps says not to.
    LPDXUTCALLBACKMSGPROC pCallbackMsgProc = g_pDXUTState.GetWindowMsgFunc();
    if( pCallbackMsgProc )
    {
        bool bNoFurtherProcessing = false;
        LRESULT nResult = pCallbackMsgProc( hWnd, uMsg, wParam, lParam, &bNoFurtherProcessing,
                                            g_pDXUTState.GetWindowMsgFuncUserContext() );
        if( bNoFurtherProcessing )
            return nResult;
    }

    switch( uMsg )
    {
        case WM_PAINT:
        {
            
            // Handle paint messages when the app is paused
            if( DXUTIsRenderingPaused() &&
                g_pDXUTState.GetDeviceObjectsCreated() && g_pDXUTState.GetDeviceObjectsReset() )
            {
                HRESULT hr;
                double fTime = DXUTGetTime();
                float fElapsedTime = DXUTGetElapsedTime();
                
                    ID3D11Device* pd3dDevice = DXUTGetD3D11Device();
                    ID3D11DeviceContext *pDeferred = DXUTGetD3D11DeviceContext();
                    if( pd3dDevice )
                    {
                        LPDXUTCALLBACKD3D11FRAMERENDER pCallbackFrameRender = g_pDXUTState.GetD3D11FrameRenderFunc();
                        if( pCallbackFrameRender != NULL &&
                            !g_pDXUTState.GetRenderingOccluded() )
                        {
                            pCallbackFrameRender( pd3dDevice,pDeferred, fTime, fElapsedTime,
                                                  g_pDXUTState.GetD3D11FrameRenderFuncUserContext() );
                        }

                        DWORD dwFlags = 0;
                        if( g_pDXUTState.GetRenderingOccluded() )
                            dwFlags = DXGI_PRESENT_TEST;
                        else
                            dwFlags = g_pDXUTState.GetCurrentDeviceSettings()->d3d11.PresentFlags;

                        IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();
                        hr = pSwapChain->Present( 0, g_pDXUTState.GetCurrentDeviceSettings()->d3d11.PresentFlags );
                        if( DXGI_STATUS_OCCLUDED == hr )
                        {
                            // There is a window covering our entire rendering area.
                            // Don't render until we're visible again.
                            g_pDXUTState.SetRenderingOccluded( true );
                        }
                        else if( SUCCEEDED( hr ) )
                        {
                            if( g_pDXUTState.GetRenderingOccluded() )
                            {
                                // Now that we're no longer occluded
                                // allow us to render again
                                g_pDXUTState.SetRenderingOccluded( false );
                            }
                        }
                    }
            }
            break;
        }

        case WM_SIZE:
            
           //if( SIZE_MINIMIZED == wParam )
           // {
           //     DXUTPause( true, true ); // Pause while we're minimized

           //     g_pDXUTState.SetMinimized( true );
           //     g_pDXUTState.SetMaximized( false );
           // }
           // else
           // {
           //     RECT rcCurrentClient;
           //     GetClientRect( DXUTGetHWND(), &rcCurrentClient );
           //     if( rcCurrentClient.top == 0 && rcCurrentClient.bottom == 0 )
           //     {
           //         // Rapidly clicking the task bar to minimize and restore a window
           //         // can cause a WM_SIZE message with SIZE_RESTORED when 
           //         // the window has actually become minimized due to rapid change
           //         // so just ignore this message
           //     }
           //     else if( SIZE_MAXIMIZED == wParam )
           //     {
           //         if( g_pDXUTState.GetMinimized() )
           //             DXUTPause( false, false ); // Unpause since we're no longer minimized
           //         g_pDXUTState.SetMinimized( false );
           //         g_pDXUTState.SetMaximized( true );
           //         DXUTCheckForWindowSizeChange();
           //         DXUTCheckForWindowChangingMonitors();
           //     }
           //     else if( SIZE_RESTORED == wParam )
           //     {
           //         //DXUTCheckForDXGIFullScreenSwitch();
           //         if( g_pDXUTState.GetMaximized() )
           //         {
           //             g_pDXUTState.SetMaximized( false );
           //             DXUTCheckForWindowSizeChange();
           //             DXUTCheckForWindowChangingMonitors();
           //         }
           //         else if( g_pDXUTState.GetMinimized() )
           //         {
           //             DXUTPause( false, false ); // Unpause since we're no longer minimized
           //             g_pDXUTState.SetMinimized( false );
           //             DXUTCheckForWindowSizeChange();
           //             DXUTCheckForWindowChangingMonitors();
           //         }
           //         else if( g_pDXUTState.GetInSizeMove() )
           //         {
           //             // If we're neither maximized nor minimized, the window size 
           //             // is changing by the user dragging the window edges.  In this 
           //             // case, we don't reset the device yet -- we wait until the 
           //             // user stops dragging, and a WM_EXITSIZEMOVE message comes.
           //         }
           //         else
           //         {
           //             // This WM_SIZE come from resizing the window via an API like SetWindowPos() so 
           //             // resize and reset the device now.
           //             DXUTCheckForWindowSizeChange();
           //             DXUTCheckForWindowChangingMonitors();
           //         }
           //     }
           // }
           // 
            break;


        case WM_GETMINMAXINFO:
            ( ( MINMAXINFO* )lParam )->ptMinTrackSize.x = DXUT_MIN_WINDOW_SIZE_X;
            ( ( MINMAXINFO* )lParam )->ptMinTrackSize.y = DXUT_MIN_WINDOW_SIZE_Y;
            break;

        case WM_ENTERSIZEMOVE:
            // Halt frame movement while the app is sizing or moving
            DXUTPause( true, true );
            g_pDXUTState.SetInSizeMove( true );
            break;

        case WM_EXITSIZEMOVE:
            DXUTPause( false, false );
            DXUTCheckForWindowSizeChange();
            //DXUTCheckForWindowChangingMonitors();
            g_pDXUTState.SetInSizeMove( false );
            break;

        case WM_MOUSEMOVE:
            break;

        case WM_SETCURSOR:
            if( DXUTIsActive() && !DXUTIsWindowed() )
            {
                 if( !g_pDXUTState.GetShowCursorWhenFullScreen() )
                        SetCursor( NULL );
                return true; // prevent Windows from setting cursor to window class cursor
            }
            break;

        case WM_ACTIVATEAPP:
            if( wParam == TRUE && !DXUTIsActive() ) // Handle only if previously not active 
            {
                g_pDXUTState.SetActive( true );

                // Enable controller rumble & input when activating app

                // The GetMinimizedWhileFullscreen() varible is used instead of !DXUTIsWindowed()
                // to handle the rare case toggling to windowed mode while the fullscreen application 
                // is minimized and thus making the pause count wrong
                if( g_pDXUTState.GetMinimizedWhileFullscreen() )
                {
                    g_pDXUTState.SetMinimizedWhileFullscreen( false );

                    if( DXUTIsAppRenderingWithD3D11() )
                    {
                      DXUTToggleFullScreen();
                    }
                }

            }
            else if( wParam == FALSE && DXUTIsActive() ) // Handle only if previously active 
            {
                g_pDXUTState.SetActive( false );

                // Disable any controller rumble & input when de-activating app

                if( !DXUTIsWindowed() )
                {
                    // Going from full screen to a minimized state 
                    ClipCursor( NULL );      // don't limit the cursor anymore
                    g_pDXUTState.SetMinimizedWhileFullscreen( true );
                }
            }
            break;

        case WM_ENTERMENULOOP:
            // Pause the app when menus are displayed
            DXUTPause( true, true );
            break;

        case WM_EXITMENULOOP:
            DXUTPause( false, false );
            break;

        case WM_MENUCHAR:
            // A menu is active and the user presses a key that does not correspond to any mnemonic or accelerator key
            // So just ignore and don't beep
            return MAKELRESULT( 0, MNC_CLOSE );
            break;

        case WM_NCHITTEST:
            // Prevent the user from selecting the menu in full screen mode
            if( !DXUTIsWindowed() )
                return HTCLIENT;
            break;

        case WM_POWERBROADCAST:
            switch( wParam )
            {
#ifndef PBT_APMQUERYSUSPEND
#define PBT_APMQUERYSUSPEND 0x0000
#endif
                case PBT_APMQUERYSUSPEND:
                    // At this point, the app should save any data for open
                    // network connections, files, etc., and prepare to go into
                    // a suspended mode.  The app can use the MsgProc callback
                    // to handle this if desired.
                    return true;

#ifndef PBT_APMRESUMESUSPEND
#define PBT_APMRESUMESUSPEND 0x0007
#endif
                case PBT_APMRESUMESUSPEND:
                    // At this point, the app should recover any data, network
                    // connections, files, etc., and resume running from when
                    // the app was suspended. The app can use the MsgProc callback
                    // to handle this if desired.

                    // QPC may lose consistency when suspending, so reset the timer
                    // upon resume.
                    g_pDXUTState.SetLastStatsUpdateTime( 0 );
                    return true;
            }
            break;

        case WM_SYSCOMMAND:
            // Prevent moving/sizing in full screen mode
            switch( ( wParam & 0xFFF0 ) )
            {
                case SC_MOVE:
                case SC_SIZE:
                case SC_MAXIMIZE:
                case SC_KEYMENU:
                    if( !DXUTIsWindowed() )
                        return 0;
                    break;
            }
            break;

        case WM_SYSKEYDOWN:
        {
            switch( wParam )
            {
                case VK_RETURN:
                {
                    // Toggle full screen upon alt-enter 
                    DWORD dwMask = ( 1 << 29 );
                    if( ( lParam & dwMask ) != 0 ) // Alt is down also
                    {
                        // Toggle the full screen/window mode
                        DXUTPause( true, true );
                        DXUTToggleFullScreen();
                        DXUTPause( false, false );
                        return 0;
                    }
                }
            }
            break;
        }

        case WM_KEYDOWN:
        {
            switch( wParam )
            {
                case VK_ESCAPE:
                {
                    if( g_pDXUTState.GetHandleEscape() )
                        SendMessage( hWnd, WM_CLOSE, 0, 0 );
                    break;
                }

                case VK_PAUSE:
                {
                    if( g_pDXUTState.GetHandlePause() )
                    {
                        bool bTimePaused = DXUTIsTimePaused();
                        bTimePaused = !bTimePaused;
                        if( bTimePaused )
                            DXUTPause( true, false );
                        else
                            DXUTPause( false, false );
                    }
                    break;
                }
            }
            break;
        }

        case WM_CLOSE:
        {
            HMENU hMenu;
            hMenu = GetMenu( hWnd );
            if( hMenu != NULL )
                DestroyMenu( hMenu );
            DestroyWindow( hWnd );
            UnregisterClass( L"Direct3DWindowClass", NULL );
            g_pDXUTState.SetHWNDFocus( NULL );
            g_pDXUTState.SetHWNDDeviceFullScreen( NULL );
            g_pDXUTState.SetHWNDDeviceWindowed( NULL );
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage( 0 );
            break;
    }

    // Don't allow the F10 key to act as a shortcut to the menu bar
    // by not passing these messages to the DefWindowProc only when
    // there's no menu present
    if( !g_pDXUTState.GetCallDefWindowProc() || g_pDXUTState.GetMenu() == NULL &&
        ( uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP ) && wParam == VK_F10 )
        return 0;
    else
        return DefWindowProc( hWnd, uMsg, wParam, lParam );
}


//--------------------------------------------------------------------------------------
// Handles app's message loop and rendering when idle.  If DXUTCreateDevice() or DXUTSetD3D*Device() 
// has not already been called, it will call DXUTCreateWindow() with the default parameters.  
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTMainLoop( HACCEL hAccel )
{
    HRESULT hr;

    // Not allowed to call this from inside the device callbacks or reenter
    if( g_pDXUTState.GetInsideDeviceCallback() || g_pDXUTState.GetInsideMainloop() )
    {
        if( ( g_pDXUTState.GetExitCode() == 0 ) || ( g_pDXUTState.GetExitCode() == 10 ) )
            g_pDXUTState.SetExitCode( 1 );
        return E_FAIL;
    }

    g_pDXUTState.SetInsideMainloop( true );

    // If DXUTCreateDevice() or DXUTSetD3D*Device() has not already been called, 
    // then call DXUTCreateDevice() with the default parameters.         
    if( !g_pDXUTState.GetDeviceCreated() )
    {
        if( g_pDXUTState.GetDeviceCreateCalled() )
        {
            if( ( g_pDXUTState.GetExitCode() == 0 ) || ( g_pDXUTState.GetExitCode() == 10 ) )
                g_pDXUTState.SetExitCode( 1 );
            return E_FAIL; // DXUTCreateDevice() must first succeed for this function to succeed
        }

        hr = DXUTCreateDevice(D3D_FEATURE_LEVEL_10_0, true, 640, 480);
        if( FAILED( hr ) )
        {
            if( ( g_pDXUTState.GetExitCode() == 0 ) || ( g_pDXUTState.GetExitCode() == 10 ) )
                g_pDXUTState.SetExitCode( 1 );
            return hr;
        }
    }

    HWND hWnd = DXUTGetHWND();

    // DXUTInit() must have been called and succeeded for this function to proceed
    // DXUTCreateWindow() or DXUTSetWindow() must have been called and succeeded for this function to proceed
    // DXUTCreateDevice() or DXUTCreateDeviceFromSettings() or DXUTSetD3D*Device() must have been called and succeeded for this function to proceed
    if( !g_pDXUTState.GetDXUTInited() || !g_pDXUTState.GetWindowCreated() || !g_pDXUTState.GetDeviceCreated() )
    {
        if( ( g_pDXUTState.GetExitCode() == 0 ) || ( g_pDXUTState.GetExitCode() == 10 ) )
            g_pDXUTState.SetExitCode( 1 );
        return E_FAIL;
    }

    // Now we're ready to receive and process Windows messages.
    bool bGotMsg;
    MSG msg;
    msg.message = WM_NULL;
    PeekMessage( &msg, NULL, 0U, 0U, PM_NOREMOVE );

    while( WM_QUIT != msg.message )
    {
        // Use PeekMessage() so we can use idle time to render the scene. 
        bGotMsg = ( PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE ) != 0 );

        if( bGotMsg )
        {
            // Translate and dispatch the message
            if( hAccel == NULL || hWnd == NULL ||
                0 == TranslateAccelerator( hWnd, hAccel, &msg ) )
            {
                TranslateMessage( &msg );
                DispatchMessage( &msg );
            }
        }
        else
        {
            // Render a frame during idle time (no messages are waiting)
            DXUTRender3DEnvironment();
        }
    }

    // Cleanup the accelerator table
    if( hAccel != NULL )
        DestroyAcceleratorTable( hAccel );

    g_pDXUTState.SetInsideMainloop( false );

    return S_OK;
}


//======================================================================================
//======================================================================================
// Direct3D section
//======================================================================================
//======================================================================================
HRESULT WINAPI DXUTCreateDevice(D3D_FEATURE_LEVEL reqFL,  bool bWindowed, int nSuggestedWidth, int nSuggestedHeight) 
{
    HRESULT hr = S_OK;
    
   
    // Not allowed to call this from inside the device callbacks
    if( g_pDXUTState.GetInsideDeviceCallback() )
        return E_FAIL;

    g_pDXUTState.SetDeviceCreateCalled( true );

    // If DXUTCreateWindow() or DXUTSetWindow() has not already been called, 
    // then call DXUTCreateWindow() with the default parameters.         
    if( !g_pDXUTState.GetWindowCreated() )
    {
        // If DXUTCreateWindow() or DXUTSetWindow() was already called and failed, then fail.
        // DXUTCreateWindow() or DXUTSetWindow() must first succeed for this function to succeed
        if( g_pDXUTState.GetWindowCreateCalled() )
            return E_FAIL;

        // If DXUTCreateWindow() or DXUTSetWindow() hasn't been called, then 
        // automatically call DXUTCreateWindow() with default params
        hr = DXUTCreateWindow();
        if( FAILED( hr ) )
            return hr;
    }
    
    DXUTDeviceSettings deviceSettings ;
    DXUTApplyDefaultDeviceSettings(&deviceSettings);
    deviceSettings.MinimumFeatureLevel = reqFL;
    deviceSettings.d3d11.sd.BufferDesc.Width = nSuggestedWidth;
    deviceSettings.d3d11.sd.BufferDesc.Height = nSuggestedHeight;
    deviceSettings.d3d11.sd.Windowed = bWindowed;
	deviceSettings.ver = DXUT_D3D11_DEVICE;

    DXUTUpdateDeviceSettingsWithOverrides(&deviceSettings); 

    // Change to a Direct3D device created from the new device settings.  
    // If there is an existing device, then either reset or recreated the scene
    hr = DXUTChangeDevice( &deviceSettings, NULL, NULL, false, true );

    if( FAILED( hr ) )
        return hr;
    
    return hr;
}

//--------------------------------------------------------------------------------------
// Tells the framework to change to a device created from the passed in device settings
// If DXUTCreateWindow() has not already been called, it will call it with the 
// default parameters.  Instead of calling this, you can call DXUTCreateDevice() 
// or DXUTSetD3D*Device() 
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTCreateDeviceFromSettings( DXUTDeviceSettings* pDeviceSettings, bool bPreserveInput,
                                             bool bClipWindowToSingleAdapter )
{
    HRESULT hr;

    g_pDXUTState.SetDeviceCreateCalled( true );

    // If DXUTCreateWindow() or DXUTSetWindow() has not already been called, 
    // then call DXUTCreateWindow() with the default parameters.         
    if( !g_pDXUTState.GetWindowCreated() )
    {
        // If DXUTCreateWindow() or DXUTSetWindow() was already called and failed, then fail.
        // DXUTCreateWindow() or DXUTSetWindow() must first succeed for this function to succeed
        if( g_pDXUTState.GetWindowCreateCalled() )
            return E_FAIL;

        // If DXUTCreateWindow() or DXUTSetWindow() hasn't been called, then 
        // automatically call DXUTCreateWindow() with default params
        hr = DXUTCreateWindow();
        if( FAILED( hr ) )
            return hr;
    }
    DXUTUpdateDeviceSettingsWithOverrides(pDeviceSettings); 

    
    // Change to a Direct3D device created from the new device settings.  
    // If there is an existing device, then either reset or recreate the scene
    hr = DXUTChangeDevice( pDeviceSettings, NULL, NULL, false, bClipWindowToSingleAdapter );
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// All device changes are sent to this function.  It looks at the current 
// device (if any) and the new device and determines the best course of action.  It 
// also remembers and restores the window state if toggling between windowed and fullscreen
// as well as sets the proper window and system state for switching to the new device.
//--------------------------------------------------------------------------------------
HRESULT DXUTChangeDevice( DXUTDeviceSettings* pNewDeviceSettings,
                          IDirect3DDevice9* pd3d9DeviceFromApp, 
                          ID3D11Device* pd3d11DeviceFromApp,
                          bool bForceRecreate, bool bClipWindowToSingleAdapter )
{
    HRESULT hr = S_OK;
    DXUTDeviceSettings* pOldDeviceSettings = g_pDXUTState.GetCurrentDeviceSettings();

    if( !pNewDeviceSettings )
        return S_FALSE;

    if ( pNewDeviceSettings->ver == DXUT_D3D11_DEVICE ) {
        hr = DXUTDelayLoadDXGI();
    }

    if( FAILED( hr ) )
        return hr;

    // Make a copy of the pNewDeviceSettings on the heap
    DXUTDeviceSettings* pNewDeviceSettingsOnHeap = new DXUTDeviceSettings;
    if( pNewDeviceSettingsOnHeap == NULL )
        return E_OUTOFMEMORY;
    memcpy( pNewDeviceSettingsOnHeap, pNewDeviceSettings, sizeof( DXUTDeviceSettings ) );
    pNewDeviceSettings = pNewDeviceSettingsOnHeap;


    g_pDXUTState.SetCurrentDeviceSettings(pNewDeviceSettingsOnHeap);

    if( FAILED( hr ) ) // the call will fail if no valid devices were found
    {
        DXUTDisplayErrorMessage( hr );
        return hr;
    }


    LPDXUTCALLBACKMODIFYDEVICESETTINGS pCallbackModifyDeviceSettings = g_pDXUTState.GetModifyDeviceSettingsFunc();
    if( pCallbackModifyDeviceSettings && pd3d9DeviceFromApp == NULL )
    {
        bool bContinue = pCallbackModifyDeviceSettings( pNewDeviceSettings,
                                                        g_pDXUTState.GetModifyDeviceSettingsFuncUserContext() );
        if( !bContinue )
        {
            // The app rejected the device change by returning false, so just use the current device if there is one.
            if( pOldDeviceSettings == NULL )
                DXUTDisplayErrorMessage( DXUTERR_NOCOMPATIBLEDEVICES );
            SAFE_DELETE( pNewDeviceSettings );
            return E_ABORT;
        }
        DXUTSnapDeviceSettingsToEnumDevice(pNewDeviceSettingsOnHeap, false); // modify the app specified settings to the closed enumerated settigns

        if( FAILED( hr ) ) // the call will fail if no valid devices were found
        {
            DXUTDisplayErrorMessage( hr );
			return hr;
        }

    }

    g_pDXUTState.SetCurrentDeviceSettings( pNewDeviceSettingsOnHeap );

    DXUTPause( true, true );

    // Create the D3D device and call the app's device callbacks
    hr = DXUTCreate3DEnvironment11( pd3d11DeviceFromApp );
    if( FAILED( hr ) )
    {
        SAFE_DELETE( pOldDeviceSettings );
        DXUTDisplayErrorMessage( hr );
        DXUTPause( false, false );
        g_pDXUTState.SetIgnoreSizeChange( false );
        return hr;
    }

	bool bNeedToResize = true;

    if( bNeedToResize )
    {
        if( bClipWindowToSingleAdapter  )
        {
            // Get the rect of the monitor attached to the adapter
            MONITORINFO miAdapter;
            miAdapter.cbSize = sizeof( MONITORINFO );
			HMONITOR hAdapterMonitor = DXUTGetMonitorFromAdapter();
            DXUTGetMonitorInfo( hAdapterMonitor, &miAdapter );

            // Get the rect of the monitor attached to the window
            MONITORINFO miWindow;
            miWindow.cbSize = sizeof( MONITORINFO );
            DXUTGetMonitorInfo( DXUTMonitorFromWindow( DXUTGetHWND(), MONITOR_DEFAULTTOPRIMARY ), &miWindow );

            // Do something reasonable if the BackBuffer size is greater than the monitor size
			int nAdapterMonitorWidth = 1920;// miAdapter.rcWork.right - miAdapter.rcWork.left;
			int nAdapterMonitorHeight = 1080;// miAdapter.rcWork.bottom - miAdapter.rcWork.top;

			int nClientWidth = pNewDeviceSettings->d3d11.sd.BufferDesc.Width;
			int nClientHeight = pNewDeviceSettings->d3d11.sd.BufferDesc.Height;

            // Get the rect of the window
            RECT rcWindow;
            GetWindowRect( DXUTGetHWNDDeviceWindowed(), &rcWindow );

            // Make a window rect with a client rect that is the same size as the backbuffer
            RECT rcResizedWindow;
            rcResizedWindow.left = 0;
            rcResizedWindow.right = nClientWidth;
            rcResizedWindow.top = 0;
            rcResizedWindow.bottom = nClientHeight;
            AdjustWindowRect( &rcResizedWindow, GetWindowLong( DXUTGetHWNDDeviceWindowed(), GWL_STYLE ),
                              g_pDXUTState.GetMenu() != NULL );

            int nWindowWidth = rcResizedWindow.right - rcResizedWindow.left;
            int nWindowHeight = rcResizedWindow.bottom - rcResizedWindow.top;

            if( nWindowWidth > nAdapterMonitorWidth )
                nWindowWidth = nAdapterMonitorWidth;
            if( nWindowHeight > nAdapterMonitorHeight )
                nWindowHeight = nAdapterMonitorHeight;

            if( rcResizedWindow.left < miAdapter.rcWork.left ||
                rcResizedWindow.top < miAdapter.rcWork.top ||
                rcResizedWindow.right > miAdapter.rcWork.right ||
                rcResizedWindow.bottom > miAdapter.rcWork.bottom )
            {
                int nWindowOffsetX = ( nAdapterMonitorWidth - nWindowWidth ) / 2;
                int nWindowOffsetY = ( nAdapterMonitorHeight - nWindowHeight ) / 2;

                rcResizedWindow.left = miAdapter.rcWork.left + nWindowOffsetX;
                rcResizedWindow.top = miAdapter.rcWork.top + nWindowOffsetY;
                rcResizedWindow.right = miAdapter.rcWork.left + nWindowOffsetX + nWindowWidth;
                rcResizedWindow.bottom = miAdapter.rcWork.top + nWindowOffsetY + nWindowHeight;
            }

            // Resize the window.  It is important to adjust the window size 
            // after resetting the device rather than beforehand to ensure 
            // that the monitor resolution is correct and does not limit the size of the new window.
            SetWindowPos( DXUTGetHWNDDeviceWindowed(), 0, rcResizedWindow.left, rcResizedWindow.top, nWindowWidth,
                          nWindowHeight, SWP_NOZORDER );
        }
    }

    // Make the window visible
    if( !IsWindowVisible( DXUTGetHWND() ) )
        ShowWindow( DXUTGetHWND(), SW_SHOW );

    SAFE_DELETE( pOldDeviceSettings );
    g_pDXUTState.SetIgnoreSizeChange( false );
    DXUTPause( false, false );
    g_pDXUTState.SetDeviceCreated( true );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Creates a DXGI factory object if one has not already been created  
//--------------------------------------------------------------------------------------
HRESULT DXUTDelayLoadDXGI()
{
    IDXGIFactory1* pDXGIFactory = g_pDXUTState.GetDXGIFactory();
    if( pDXGIFactory == NULL )
    {
        DXUT_Dynamic_CreateDXGIFactory1( __uuidof( IDXGIFactory1 ), ( LPVOID* )&pDXGIFactory );
        g_pDXUTState.SetDXGIFactory( pDXGIFactory );
        if( pDXGIFactory == NULL )
        {
            // If still NULL, then DXGI is not availible
            g_pDXUTState.SetD3D11Available( false );
            return DXUTERR_NODIRECT3D11;
        }

        // DXGI 1.1 implies Direct3D 11

        g_pDXUTState.SetD3D11Available( true );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Updates the device settings with default values..  
//--------------------------------------------------------------------------------------
void DXUTUpdateDeviceSettingsWithOverrides( DXUTDeviceSettings* pDeviceSettings )
{
    // Override with settings from the command line
    if( g_pDXUTState.GetOverrideWidth() != 0 )
    {
        pDeviceSettings->d3d11.sd.BufferDesc.Width = g_pDXUTState.GetOverrideWidth();
    }
    if( g_pDXUTState.GetOverrideHeight() != 0 )
    {
        pDeviceSettings->d3d11.sd.BufferDesc.Height = g_pDXUTState.GetOverrideHeight();
    }

    if( g_pDXUTState.GetOverrideAdapterOrdinal() != -1 )
    {
        pDeviceSettings->d3d11.AdapterOrdinal = g_pDXUTState.GetOverrideAdapterOrdinal();
    }

    if( g_pDXUTState.GetOverrideFullScreen() )
    {
        pDeviceSettings->d3d11.sd.Windowed = FALSE;
    }

    if( g_pDXUTState.GetOverrideWindowed() ) {
        pDeviceSettings->d3d11.sd.Windowed = TRUE;
    }

    if( g_pDXUTState.GetOverrideForceHAL() )
    {
        pDeviceSettings->d3d11.DriverType = D3D_DRIVER_TYPE_HARDWARE;
    }

    if( g_pDXUTState.GetOverrideForceREF() )
    {
        pDeviceSettings->d3d11.DriverType = D3D_DRIVER_TYPE_REFERENCE;
    }

    if( g_pDXUTState.GetOverrideForceVsync() == 0 )
    {
        pDeviceSettings->d3d11.SyncInterval = 0;
    }
    else if( g_pDXUTState.GetOverrideForceVsync() == 1 )
    {
        pDeviceSettings->d3d11.SyncInterval = 1;
    }

    if( g_pDXUTState.GetOverrideForceAPI() != -1 )
    {
        if( g_pDXUTState.GetOverrideForceAPI() == 11 )
        {
            pDeviceSettings->ver = DXUT_D3D11_DEVICE;
        }
    }
    
    if (g_pDXUTState.GetOverrideForceFeatureLevel() != 0) {
        pDeviceSettings->d3d11.DeviceFeatureLevel = (D3D_FEATURE_LEVEL)g_pDXUTState.GetOverrideForceFeatureLevel();
    }
}


//--------------------------------------------------------------------------------------
// Allows the app to explictly state if it supports D3D9 or D3D11.  Typically
// calling this is not needed as DXUT will auto-detect this based on the callbacks set.
//--------------------------------------------------------------------------------------
void WINAPI DXUTSetD3DVersionSupport( bool bAppCanUseD3D9,  bool bAppCanUseD3D11 )
{
    g_pDXUTState.SetUseD3DVersionOverride( true );
    g_pDXUTState.SetAppSupportsD3D9Override( bAppCanUseD3D9 );
    g_pDXUTState.SetAppSupportsD3D11Override( bAppCanUseD3D11 );
}

//--------------------------------------------------------------------------------------
// Gives the D3D device a cursor with image and hotspot from hCursor.
//--------------------------------------------------------------------------------------
HRESULT DXUTSetD3D9DeviceCursor( IDirect3DDevice9* pd3dDevice, HCURSOR hCursor, bool bAddWatermark )
{
    HRESULT hr = E_FAIL;
    ICONINFO iconinfo;
    bool bBWCursor = false;
    LPDIRECT3DSURFACE9 pCursorSurface = NULL;
    HDC hdcColor = NULL;
    HDC hdcMask = NULL;
    HDC hdcScreen = NULL;
    BITMAP bm;
    DWORD dwWidth = 0;
    DWORD dwHeightSrc = 0;
    DWORD dwHeightDest = 0;
    COLORREF crColor;
    COLORREF crMask;
    UINT x;
    UINT y;
    BITMAPINFO bmi;
    COLORREF* pcrArrayColor = NULL;
    COLORREF* pcrArrayMask = NULL;
    DWORD* pBitmap;
    HGDIOBJ hgdiobjOld;

    ZeroMemory( &iconinfo, sizeof( iconinfo ) );
    if( !GetIconInfo( hCursor, &iconinfo ) )
        goto End;

    if( 0 == GetObject( ( HGDIOBJ )iconinfo.hbmMask, sizeof( BITMAP ), ( LPVOID )&bm ) )
        goto End;
    dwWidth = bm.bmWidth;
    dwHeightSrc = bm.bmHeight;

    if( iconinfo.hbmColor == NULL )
    {
        bBWCursor = TRUE;
        dwHeightDest = dwHeightSrc / 2;
    }
    else
    {
        bBWCursor = FALSE;
        dwHeightDest = dwHeightSrc;
    }

    // Create a surface for the fullscreen cursor
    if( FAILED( hr = pd3dDevice->CreateOffscreenPlainSurface( dwWidth, dwHeightDest,
                                                              D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &pCursorSurface,
                                                              NULL ) ) )
    {
        goto End;
    }

    pcrArrayMask = new DWORD[dwWidth * dwHeightSrc];

    ZeroMemory( &bmi, sizeof( bmi ) );
    bmi.bmiHeader.biSize = sizeof( bmi.bmiHeader );
    bmi.bmiHeader.biWidth = dwWidth;
    bmi.bmiHeader.biHeight = dwHeightSrc;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    hdcScreen = GetDC( NULL );
    hdcMask = CreateCompatibleDC( hdcScreen );
    if( hdcMask == NULL )
    {
        hr = E_FAIL;
        goto End;
    }
    hgdiobjOld = SelectObject( hdcMask, iconinfo.hbmMask );
    GetDIBits( hdcMask, iconinfo.hbmMask, 0, dwHeightSrc,
               pcrArrayMask, &bmi, DIB_RGB_COLORS );
    SelectObject( hdcMask, hgdiobjOld );

    if( !bBWCursor )
    {
        pcrArrayColor = new DWORD[dwWidth * dwHeightDest];
        hdcColor = CreateCompatibleDC( hdcScreen );
        if( hdcColor == NULL )
        {
            hr = E_FAIL;
            goto End;
        }
        SelectObject( hdcColor, iconinfo.hbmColor );
        GetDIBits( hdcColor, iconinfo.hbmColor, 0, dwHeightDest,
                   pcrArrayColor, &bmi, DIB_RGB_COLORS );
    }

    // Transfer cursor image into the surface
    D3DLOCKED_RECT lr;
    pCursorSurface->LockRect( &lr, NULL, 0 );
    pBitmap = ( DWORD* )lr.pBits;
    for( y = 0; y < dwHeightDest; y++ )
    {
        for( x = 0; x < dwWidth; x++ )
        {
            if( bBWCursor )
            {
                crColor = pcrArrayMask[dwWidth * ( dwHeightDest - 1 - y ) + x];
                crMask = pcrArrayMask[dwWidth * ( dwHeightSrc - 1 - y ) + x];
            }
            else
            {
                crColor = pcrArrayColor[dwWidth * ( dwHeightDest - 1 - y ) + x];
                crMask = pcrArrayMask[dwWidth * ( dwHeightDest - 1 - y ) + x];
            }
            if( crMask == 0 )
                pBitmap[dwWidth * y + x] = 0xff000000 | crColor;
            else
                pBitmap[dwWidth * y + x] = 0x00000000;

            // It may be helpful to make the D3D cursor look slightly 
            // different from the Windows cursor so you can distinguish 
            // between the two when developing/testing code.  When
            // bAddWatermark is TRUE, the following code adds some
            // small grey "D3D" characters to the upper-left corner of
            // the D3D cursor image.
            if( bAddWatermark && x < 12 && y < 5 )
            {
                // 11.. 11.. 11.. .... CCC0
                // 1.1. ..1. 1.1. .... A2A0
                // 1.1. .1.. 1.1. .... A4A0
                // 1.1. ..1. 1.1. .... A2A0
                // 11.. 11.. 11.. .... CCC0

                const WORD wMask[5] = { 0xccc0, 0xa2a0, 0xa4a0, 0xa2a0, 0xccc0 };
                if( wMask[y] & (1 << (15 - x)) )
                {
                    pBitmap[dwWidth*y + x] |= 0xff808080;
                }
            }
        }
    }
    pCursorSurface->UnlockRect();

    // Set the device cursor
    if( FAILED( hr = pd3dDevice->SetCursorProperties( iconinfo.xHotspot,
                                                      iconinfo.yHotspot, pCursorSurface ) ) )
    {
        goto End;
    }

    hr = S_OK;

End:
    if( iconinfo.hbmMask != NULL )
        DeleteObject( iconinfo.hbmMask );
    if( iconinfo.hbmColor != NULL )
        DeleteObject( iconinfo.hbmColor );
    if( hdcScreen != NULL )
        ReleaseDC( NULL, hdcScreen );
    if( hdcColor != NULL )
        DeleteDC( hdcColor );
    if( hdcMask != NULL )
        DeleteDC( hdcMask );
    SAFE_DELETE_ARRAY( pcrArrayColor );
    SAFE_DELETE_ARRAY( pcrArrayMask );
    SAFE_RELEASE( pCursorSurface );
    return hr;
}

//--------------------------------------------------------------------------------------
// Sets the viewport, render target view, and depth stencil view.
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTSetupD3D11Views( ID3D11DeviceContext* pd3dDeviceContext )
{
    HRESULT hr = S_OK;

    // Setup the viewport to match the backbuffer
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)DXUTGetDXGIBackBufferSurfaceDesc()->Width;
    vp.Height = (FLOAT)DXUTGetDXGIBackBufferSurfaceDesc()->Height;
    vp.MinDepth = 0;
    vp.MaxDepth = 1;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    pd3dDeviceContext->RSSetViewports( 1, &vp );

    // Set the render targets
    ID3D11RenderTargetView* pRTV = g_pDXUTState.GetD3D11RenderTargetView();
    ID3D11DepthStencilView* pDSV = g_pDXUTState.GetD3D11DepthStencilView();
    pd3dDeviceContext->OMSetRenderTargets( 1, &pRTV, pDSV );

    return hr;
}


//--------------------------------------------------------------------------------------
// Creates a render target view, and depth stencil texture and view.
//--------------------------------------------------------------------------------------
HRESULT DXUTCreateD3D11Views( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
                             DXUTDeviceSettings* pDeviceSettings )
{
    HRESULT hr = S_OK;
    IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();
    ID3D11DepthStencilView* pDSV = NULL;
    ID3D11RenderTargetView* pRTV = NULL;

    // Get the back buffer and desc
    ID3D11Texture2D* pBackBuffer;
    hr = pSwapChain->GetBuffer( 0, __uuidof( *pBackBuffer ), ( LPVOID* )&pBackBuffer );
    if( FAILED( hr ) )
        return hr;
    D3D11_TEXTURE2D_DESC backBufferSurfaceDesc;
    pBackBuffer->GetDesc( &backBufferSurfaceDesc );

    // Create the render target view
    hr = pd3dDevice->CreateRenderTargetView( pBackBuffer, NULL, &pRTV );
    SAFE_RELEASE( pBackBuffer );
    if( FAILED( hr ) )
        return hr;
    g_pDXUTState.SetD3D11RenderTargetView( pRTV );


        // Create depth stencil texture
        ID3D11Texture2D* pDepthStencil = NULL;
        D3D11_TEXTURE2D_DESC descDepth;
        descDepth.Width = backBufferSurfaceDesc.Width;
        descDepth.Height = backBufferSurfaceDesc.Height;
        descDepth.MipLevels = 1;
        descDepth.ArraySize = 1;
		descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        descDepth.SampleDesc.Count = 1;
        descDepth.SampleDesc.Quality = 0;
        descDepth.Usage = D3D11_USAGE_DEFAULT;
        descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        descDepth.CPUAccessFlags = 0;
        descDepth.MiscFlags = 0;
        hr = pd3dDevice->CreateTexture2D( &descDepth, NULL, &pDepthStencil );
        if( FAILED( hr ) )
            return hr;
        g_pDXUTState.SetD3D11DepthStencil( pDepthStencil );

        // Create the depth stencil view
        D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
        descDSV.Format = descDepth.Format;
        descDSV.Flags = 0;
        if( descDepth.SampleDesc.Count > 1 )
            descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
        else
            descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        descDSV.Texture2D.MipSlice = 0;
        hr = pd3dDevice->CreateDepthStencilView( pDepthStencil, &descDSV, &pDSV );
        if( FAILED( hr ) )
            return hr;
        g_pDXUTState.SetD3D11DepthStencilView( pDSV );

    hr = DXUTSetupD3D11Views( pd3dImmediateContext );
    if( FAILED( hr ) )
        return hr;

    return hr;
}


//--------------------------------------------------------------------------------------
// Creates the 3D environment
//--------------------------------------------------------------------------------------
HRESULT DXUTCreate3DEnvironment11( ID3D11Device* pd3d11DeviceFromApp )
{
    HRESULT hr = S_OK;

    ID3D11Device* pd3d11Device = NULL;
    ID3D11DeviceContext* pd3dImmediateContext = NULL;
    D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;

    IDXGISwapChain* pSwapChain = NULL;
        
    IDXGIFactory1* pDXGIFactory = DXUTGetDXGIFactory();
    assert( pDXGIFactory != NULL );
    hr = pDXGIFactory->MakeWindowAssociation( DXUTGetHWND(), 0  );

    // Only create a Direct3D device if one hasn't been supplied by the app
    if( pd3d11DeviceFromApp == NULL )
    {
        // Try to create the device with the chosen settings
        IDXGIAdapter1* pAdapter = NULL;

        hr = S_OK;

        if( SUCCEEDED( hr ) )
        {
			D3D_FEATURE_LEVEL DeviceFeatureLevel = D3D_FEATURE_LEVEL_11_0;
            hr = DXUT_Dynamic_D3D11CreateDevice( NULL,
												D3D_DRIVER_TYPE_HARDWARE,
                                                 ( HMODULE )0,
                                                 0,
												 &DeviceFeatureLevel,
                                                 1,
                                                 D3D11_SDK_VERSION,
                                                 &pd3d11Device,
                                                 &FeatureLevel,
                                                 &pd3dImmediateContext
                                                 );
        }

        if( SUCCEEDED( hr ) )
        {
            IDXGIDevice1* pDXGIDev = NULL;
            hr = pd3d11Device->QueryInterface( __uuidof( IDXGIDevice1 ), ( LPVOID* )&pDXGIDev );
            if( SUCCEEDED( hr ) && pDXGIDev )
            {
                if ( pAdapter == NULL ) 
                {
                    IDXGIAdapter *pTempAdapter;
                    pDXGIDev->GetAdapter( &pTempAdapter );
                    pTempAdapter->QueryInterface( __uuidof( IDXGIAdapter1 ), (LPVOID*) &pAdapter );
                    pAdapter->GetParent( __uuidof( IDXGIFactory1 ), (LPVOID*) &pDXGIFactory );
                    SAFE_RELEASE ( pTempAdapter );
                    g_pDXUTState.SetDXGIFactory( pDXGIFactory );
                }
            }
            SAFE_RELEASE( pDXGIDev );
            g_pDXUTState.SetDXGIAdapter( pAdapter );
        }

        if( FAILED( hr ) )
        {
            return DXUTERR_CREATINGDEVICE;
        }
        // set default render state to msaa enabled
        D3D11_RASTERIZER_DESC drd = {
            D3D11_FILL_SOLID, //D3D11_FILL_MODE FillMode;
            D3D11_CULL_BACK,//D3D11_CULL_MODE CullMode;
            FALSE, //BOOL FrontCounterClockwise;
            0, //INT DepthBias;
            0.0f,//FLOAT DepthBiasClamp;
            0.0f,//FLOAT SlopeScaledDepthBias;
            TRUE,//BOOL DepthClipEnable;
            FALSE,//BOOL ScissorEnable;
            TRUE,//BOOL MultisampleEnable;
            FALSE//BOOL AntialiasedLineEnable;        
        };
        ID3D11RasterizerState* pRS = NULL;
        hr = pd3d11Device->CreateRasterizerState(&drd, &pRS);
        if ( FAILED( hr ) )
        {
            return DXUTERR_CREATINGDEVICE;
        }
        g_pDXUTState.SetD3D11RasterizerState(pRS);
        pd3dImmediateContext->RSSetState(pRS);

        // Enumerate its outputs.
        UINT OutputCount, iOutput;
        for( OutputCount = 0; ; ++OutputCount )
        {
            IDXGIOutput* pOutput = 0;
			if (FAILED( pAdapter->EnumOutputs(OutputCount, &pOutput)))
                break;
            SAFE_RELEASE( pOutput );
        }
        IDXGIOutput** ppOutputArray = new IDXGIOutput*[OutputCount];
        if( !ppOutputArray )
            return E_OUTOFMEMORY;
        for( iOutput = 0; iOutput < OutputCount; ++iOutput )
            pAdapter->EnumOutputs( iOutput, ppOutputArray + iOutput );
        g_pDXUTState.SetDXGIOutputArray( ppOutputArray );
        g_pDXUTState.SetDXGIOutputArraySize( OutputCount );

        // Create the swapchain
		//后备缓冲个数
		DXGI_SWAP_CHAIN_DESC desc;
		ZeroMemory(&desc, sizeof(DXGI_SWAP_CHAIN_DESC));
		desc.BufferCount = 1;
		//后备缓冲大小
		desc.BufferDesc.Width = 800;
		desc.BufferDesc.Height = 600;
		//像素格式 归一化
		desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		//立即刷新
		desc.BufferDesc.RefreshRate.Numerator = 60;
		desc.BufferDesc.RefreshRate.Denominator = 1;
		//扫描线设置
		desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		//设为渲染目标缓冲
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		//窗口模式
		desc.Windowed = TRUE;
		desc.OutputWindow = g_pDXUTState.m_HWNDDeviceWindowed;

		//不采用多重采样
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		//后备缓冲内容呈现屏幕后，放弃其内容
		desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		//不设置标志
		desc.Flags = 0;

		hr = pDXGIFactory->CreateSwapChain(pd3d11Device, &desc, &pSwapChain);
        if( FAILED( hr ) )
        {
            return DXUTERR_CREATINGDEVICE;
        }
    }
    else
    {
        pd3d11DeviceFromApp->AddRef();
        pd3d11Device = pd3d11DeviceFromApp;
    }

    g_pDXUTState.SetD3D11Device( pd3d11Device );
    g_pDXUTState.SetD3D11DeviceContext( pd3dImmediateContext );
    g_pDXUTState.SetD3D11FeatureLevel( FeatureLevel );
    g_pDXUTState.SetDXGISwapChain( pSwapChain );

    // Update back buffer desc before calling app's device callbacks
   DXUTUpdateBackBufferDesc();

    // Setup cursor based on current settings (window/fullscreen mode, show cursor state, clip cursor state)
    DXUTSetupCursor();

    // Call the app's device created callback if non-NULL
    const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc = DXUTGetDXGIBackBufferSurfaceDesc();
    g_pDXUTState.SetInsideDeviceCallback( true );
    LPDXUTCALLBACKD3D11DEVICECREATED pCallbackDeviceCreated = g_pDXUTState.GetD3D11DeviceCreatedFunc();
    hr = S_OK;
    if( pCallbackDeviceCreated != NULL )
        hr = pCallbackDeviceCreated( DXUTGetD3D11Device(), pBackBufferSurfaceDesc,
                                     g_pDXUTState.GetD3D11DeviceCreatedFuncUserContext() );
    g_pDXUTState.SetInsideDeviceCallback( false );
    if( DXUTGetD3D11Device() == NULL ) // Handle DXUTShutdown from inside callback
        return E_FAIL;
    if( FAILED( hr ) )
    {
        return ( hr == DXUTERR_MEDIANOTFOUND ) ? DXUTERR_MEDIANOTFOUND : DXUTERR_CREATINGDEVICEOBJECTS;
    }
    g_pDXUTState.SetDeviceObjectsCreated( true );

    // Setup the render target view and viewport
    hr = DXUTCreateD3D11Views( pd3d11Device, pd3dImmediateContext, NULL );
    if( FAILED( hr ) )
    {
        return DXUTERR_CREATINGDEVICEOBJECTS;
    }

    // Create performance counters
    //DXUTCreateD3D11Counters( pd3d11Device );

    // Call the app's swap chain reset callback if non-NULL
    g_pDXUTState.SetInsideDeviceCallback( true );
    LPDXUTCALLBACKD3D11SWAPCHAINRESIZED pCallbackSwapChainResized = g_pDXUTState.GetD3D11SwapChainResizedFunc();
    hr = S_OK;
    if( pCallbackSwapChainResized != NULL )
        hr = pCallbackSwapChainResized( DXUTGetD3D11Device(), pSwapChain, pBackBufferSurfaceDesc,
                                        g_pDXUTState.GetD3D11SwapChainResizedFuncUserContext() );
    g_pDXUTState.SetInsideDeviceCallback( false );
    if( DXUTGetD3D11Device() == NULL ) // Handle DXUTShutdown from inside callback
        return E_FAIL;
    if( FAILED( hr ) )
    {
        return ( hr == DXUTERR_MEDIANOTFOUND ) ? DXUTERR_MEDIANOTFOUND : DXUTERR_RESETTINGDEVICEOBJECTS;
    }
    g_pDXUTState.SetDeviceObjectsReset( true );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Resets the 3D environment by:
//      - Calls the device lost callback 
//      - Resets the device
//      - Stores the back buffer description
//      - Sets up the full screen Direct3D cursor if requested
//      - Calls the device reset callback 
//--------------------------------------------------------------------------------------
HRESULT DXUTReset3DEnvironment11()
{
    HRESULT hr;

    g_pDXUTState.SetDeviceObjectsReset( false );
    DXUTPause( true, true );

    bool bDeferredDXGIAction = false;
    DXUTDeviceSettings* pDeviceSettings = g_pDXUTState.GetCurrentDeviceSettings();
    IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();
    assert( pSwapChain != NULL );

    DXGI_SWAP_CHAIN_DESC SCDesc;
    pSwapChain->GetDesc( &SCDesc );

    // Resize backbuffer and target of the swapchain in case they have changed.
    // For windowed mode, use the client rect as the desired size. Unlike D3D9,
    // we can't use 0 for width or height.  Therefore, fill in the values from
    // the window size. For fullscreen mode, the width and height should have
    // already been filled with the desktop resolution, so don't change it.
    if( pDeviceSettings->d3d11.sd.Windowed && SCDesc.Windowed )
    {
        RECT rcWnd;
        GetClientRect( DXUTGetHWND(), &rcWnd );
        pDeviceSettings->d3d11.sd.BufferDesc.Width = rcWnd.right - rcWnd.left;
        pDeviceSettings->d3d11.sd.BufferDesc.Height = rcWnd.bottom - rcWnd.top;
    }

    // If the app wants to switch from windowed to fullscreen or vice versa,
    // call the swapchain's SetFullscreenState
    // mode.
    if( SCDesc.Windowed != pDeviceSettings->d3d11.sd.Windowed )
    {
        // Set the fullscreen state
        if( pDeviceSettings->d3d11.sd.Windowed )
        {
            V_RETURN( pSwapChain->SetFullscreenState( FALSE, NULL ) );
            bDeferredDXGIAction = true;
        }
        else
        {
            // Set fullscreen state by setting the display mode to fullscreen, then changing the resolution
            // to the desired value.

            // SetFullscreenState causes a WM_SIZE message to be sent to the window.  The WM_SIZE message calls
            // DXUTCheckForDXGIBufferChange which normally stores the new height and width in 
            // pDeviceSettings->d3d11.sd.BufferDesc.  SetDoNotStoreBufferSize tells DXUTCheckForDXGIBufferChange
            // not to store the height and width so that we have the correct values when calling ResizeTarget.

            g_pDXUTState.SetDoNotStoreBufferSize( true );
            V_RETURN( pSwapChain->SetFullscreenState( TRUE, NULL ) );
            g_pDXUTState.SetDoNotStoreBufferSize( false );

            V_RETURN( pSwapChain->ResizeTarget( &pDeviceSettings->d3d11.sd.BufferDesc ) );
            bDeferredDXGIAction = true;
        }
    }
    else
    {
        if( pDeviceSettings->d3d11.sd.BufferDesc.Width == SCDesc.BufferDesc.Width &&
            pDeviceSettings->d3d11.sd.BufferDesc.Height == SCDesc.BufferDesc.Height &&
            pDeviceSettings->d3d11.sd.BufferDesc.Format != SCDesc.BufferDesc.Format )
        {
            DXUTResizeDXGIBuffers( 0, 0, !pDeviceSettings->d3d11.sd.Windowed );
            bDeferredDXGIAction = true;
        }
        else if( pDeviceSettings->d3d11.sd.BufferDesc.Width != SCDesc.BufferDesc.Width ||
                 pDeviceSettings->d3d11.sd.BufferDesc.Height != SCDesc.BufferDesc.Height )
        {
            V_RETURN( pSwapChain->ResizeTarget( &pDeviceSettings->d3d11.sd.BufferDesc ) );
            bDeferredDXGIAction = true;
        }
    }

    // If no deferred DXGI actions are to take place, mark the device as reset.
    // If there is a deferred DXGI action, then the device isn't reset until DXGI sends us a 
    // window message.  Only then can we mark the device as reset.
    if( !bDeferredDXGIAction )
        g_pDXUTState.SetDeviceObjectsReset( true );
    DXUTPause( false, false );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the 3D environment by:
//      - Checking if the device is lost and trying to reset it if it is
//      - Get the elapsed time since the last frame
//      - Calling the app's framemove and render callback
//      - Calling Present()
//--------------------------------------------------------------------------------------
void DXUTRender3DEnvironment11()
{
    HRESULT hr;

    ID3D11Device* pd3dDevice = DXUTGetD3D11Device();
    if( NULL == pd3dDevice )
        return;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    if( NULL == pd3dImmediateContext )
        return;

    IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();
    if( NULL == pSwapChain )
        return;

    if( DXUTIsRenderingPaused() || !DXUTIsActive() || g_pDXUTState.GetRenderingOccluded() )
    {
        // Window is minimized/paused/occluded/or not exclusive so yield CPU time to other processes
        Sleep( 50 );
    }

    // Get the app's time, in seconds. Skip rendering if no time elapsed
	double fTime = 0, fAbsTime = 0; float fElapsedTime = 0;
    // Store the time for the app
    if( g_pDXUTState.GetConstantFrameTime() )
    {
        fElapsedTime = g_pDXUTState.GetTimePerFrame();
		fTime = 0;// DXUTGetTime() + fElapsedTime;
    }

    g_pDXUTState.SetTime( fTime );
    g_pDXUTState.SetAbsoluteTime( fAbsTime );
    g_pDXUTState.SetElapsedTime( fElapsedTime );

    // Start Performance Counters

    // Update the FPS stats
    DXUTUpdateFrameStats();

    DXUTHandleTimers();

    // Animate the scene by calling the app's frame move callback
    LPDXUTCALLBACKFRAMEMOVE pCallbackFrameMove = g_pDXUTState.GetFrameMoveFunc();
    if( pCallbackFrameMove != NULL )
    {
        pCallbackFrameMove( fTime, fElapsedTime, g_pDXUTState.GetFrameMoveFuncUserContext() );
        pd3dDevice = DXUTGetD3D11Device();
        if( NULL == pd3dDevice ) // Handle DXUTShutdown from inside callback
            return;
    }

    if( !g_pDXUTState.GetRenderingPaused() )
    {
        // Render the scene by calling the app's render callback
        LPDXUTCALLBACKD3D11FRAMERENDER pCallbackFrameRender = g_pDXUTState.GetD3D11FrameRenderFunc();
        if( pCallbackFrameRender != NULL && !g_pDXUTState.GetRenderingOccluded() )
        {
            pCallbackFrameRender( pd3dDevice, pd3dImmediateContext, fTime, fElapsedTime,
                                  g_pDXUTState.GetD3D11FrameRenderFuncUserContext() );
            
            pd3dDevice = DXUTGetD3D11Device();
            if( NULL == pd3dDevice ) // Handle DXUTShutdown from inside callback
                return;
        }

#if defined(DEBUG) || defined(_DEBUG)
        // The back buffer should always match the client rect 
        // if the Direct3D backbuffer covers the entire window
        RECT rcClient;
        GetClientRect( DXUTGetHWND(), &rcClient );
        if( !IsIconic( DXUTGetHWND() ) )
        {
            GetClientRect( DXUTGetHWND(), &rcClient );
            
           // assert( DXUTGetDXGIBackBufferSurfaceDesc()->Width == (UINT)rcClient.right );
           // assert( DXUTGetDXGIBackBufferSurfaceDesc()->Height == (UINT)rcClient.bottom );
        }
#endif
    }

    if ( g_pDXUTState.GetSaveScreenShot() ) {
        DXUTSnapD3D11Screenshot( g_pDXUTState.GetScreenShotName(), D3DX11_IFF_BMP );
    }
    if ( g_pDXUTState.GetExitAfterScreenShot() ) {
        DXUTShutdown();
        return;
    }

    DWORD dwFlags = 0;
    if( g_pDXUTState.GetRenderingOccluded() )
        dwFlags = DXGI_PRESENT_TEST;
    else
        dwFlags = g_pDXUTState.GetCurrentDeviceSettings()->d3d11.PresentFlags;
    UINT SyncInterval = g_pDXUTState.GetCurrentDeviceSettings()->d3d11.SyncInterval;

    // Show the frame on the primary surface.
    hr = pSwapChain->Present( SyncInterval, dwFlags );
    if( DXGI_STATUS_OCCLUDED == hr )
    {
        // There is a window covering our entire rendering area.
        // Don't render until we're visible again.
        g_pDXUTState.SetRenderingOccluded( true );
    }
    else if( DXGI_ERROR_DEVICE_RESET == hr )
    {
        // If a mode change happened, we must reset the device
        if( FAILED( hr = DXUTReset3DEnvironment11() ) )
        {
            if( DXUTERR_RESETTINGDEVICEOBJECTS == hr ||
                DXUTERR_MEDIANOTFOUND == hr )
            {
                DXUTDisplayErrorMessage( hr );
                DXUTShutdown();
                return;
            }
            else
            {
                // Reset failed, but the device wasn't lost so something bad happened, 
                // so recreate the device to try to recover
                DXUTDeviceSettings* pDeviceSettings = g_pDXUTState.GetCurrentDeviceSettings();
                if( FAILED( DXUTChangeDevice( pDeviceSettings, NULL, NULL, true, false ) ) )
                {
                    DXUTShutdown();
                    return;
                }

                // How to handle display orientation changes in full-screen mode?
            }
        }
    }
    else if( DXGI_ERROR_DEVICE_REMOVED == hr )
    {
        // Use a callback to ask the app if it would like to find a new device.  
        // If no device removed callback is set, then look for a new device
        if( FAILED( DXUTHandleDeviceRemoved() ) )
        {
            // Perhaps get more information from pD3DDevice->GetDeviceRemovedReason()?
            DXUTDisplayErrorMessage( DXUTERR_DEVICEREMOVED );
            DXUTShutdown();
            return;
        }
    }
    else if( SUCCEEDED( hr ) )
    {
        if( g_pDXUTState.GetRenderingOccluded() )
        {
            // Now that we're no longer occluded
            // allow us to render again
            g_pDXUTState.SetRenderingOccluded( false );
        }
    }

    // Update current frame #
    int nFrame = g_pDXUTState.GetCurrentFrameNumber();
    nFrame++;
    g_pDXUTState.SetCurrentFrameNumber( nFrame );


    // Update the D3D11 counter stats
    //DXUTUpdateD3D11CounterStats();

    // Check to see if the app should shutdown due to cmdline
    if( g_pDXUTState.GetOverrideQuitAfterFrame() != 0 )
    {
        if( nFrame > g_pDXUTState.GetOverrideQuitAfterFrame() )
            DXUTShutdown();
    }

    return;
}

void ClearD3D11DeviceContext( ID3D11DeviceContext* pd3dDeviceContext )
{
    // Unbind all objects from the immediate context
    if (pd3dDeviceContext == NULL) return;

    ID3D11ShaderResourceView* pSRVs[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
    ID3D11RenderTargetView* pRTVs[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
    ID3D11DepthStencilView* pDSV = NULL;
    ID3D11Buffer* pBuffers[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
    ID3D11SamplerState* pSamplers[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
    UINT StrideOffset[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

    // Shaders
    pd3dDeviceContext->VSSetShader( NULL, NULL, 0 );
    pd3dDeviceContext->HSSetShader( NULL, NULL, 0 );
    pd3dDeviceContext->DSSetShader( NULL, NULL, 0 );
    pd3dDeviceContext->GSSetShader( NULL, NULL, 0 );
    pd3dDeviceContext->PSSetShader( NULL, NULL, 0 );

    // IA clear
    pd3dDeviceContext->IASetVertexBuffers( 0, 16, pBuffers, StrideOffset, StrideOffset );
    pd3dDeviceContext->IASetIndexBuffer( NULL, DXGI_FORMAT_R16_UINT, 0 );
    pd3dDeviceContext->IASetInputLayout( NULL );

    // Constant buffers
    pd3dDeviceContext->VSSetConstantBuffers( 0, 14, pBuffers );
    pd3dDeviceContext->HSSetConstantBuffers( 0, 14, pBuffers );
    pd3dDeviceContext->DSSetConstantBuffers( 0, 14, pBuffers );
    pd3dDeviceContext->GSSetConstantBuffers( 0, 14, pBuffers );
    pd3dDeviceContext->PSSetConstantBuffers( 0, 14, pBuffers );

    // Resources
    pd3dDeviceContext->VSSetShaderResources( 0, 16, pSRVs );
    pd3dDeviceContext->HSSetShaderResources( 0, 16, pSRVs );
    pd3dDeviceContext->DSSetShaderResources( 0, 16, pSRVs );
    pd3dDeviceContext->GSSetShaderResources( 0, 16, pSRVs );
    pd3dDeviceContext->PSSetShaderResources( 0, 16, pSRVs );

    // Samplers
    pd3dDeviceContext->VSSetSamplers( 0, 16, pSamplers );
    pd3dDeviceContext->HSSetSamplers( 0, 16, pSamplers );
    pd3dDeviceContext->DSSetSamplers( 0, 16, pSamplers );
    pd3dDeviceContext->GSSetSamplers( 0, 16, pSamplers );
    pd3dDeviceContext->PSSetSamplers( 0, 16, pSamplers );

    // Render targets
    pd3dDeviceContext->OMSetRenderTargets( 8, pRTVs, pDSV );

    // States
    FLOAT blendFactor[4] = { 0,0,0,0 };
    pd3dDeviceContext->OMSetBlendState( NULL, blendFactor, 0xFFFFFFFF );
    pd3dDeviceContext->OMSetDepthStencilState( NULL, 0 );
    pd3dDeviceContext->RSSetState( NULL );
}

//--------------------------------------------------------------------------------------
// Low level keyboard hook to disable Windows key to prevent accidental task switching.  
//--------------------------------------------------------------------------------------
LRESULT CALLBACK DXUTLowLevelKeyboardProc( int nCode, WPARAM wParam, LPARAM lParam )
{
    if( nCode < 0 || nCode != HC_ACTION )  // do not process message 
        return CallNextHookEx( g_pDXUTState.GetKeyboardHook(), nCode, wParam, lParam );

    bool bEatKeystroke = false;
    KBDLLHOOKSTRUCT* p = ( KBDLLHOOKSTRUCT* )lParam;
    switch( wParam )
    {
        case WM_KEYDOWN:
        case WM_KEYUP:
            {
                bEatKeystroke = ( !g_pDXUTState.GetAllowShortcutKeys() &&
                                  ( p->vkCode == VK_LWIN || p->vkCode == VK_RWIN ) );
                break;
            }
    }

    if( bEatKeystroke )
        return 1;
    else
        return CallNextHookEx( g_pDXUTState.GetKeyboardHook(), nCode, wParam, lParam );
}

//--------------------------------------------------------------------------------------
// Pauses time or rendering.  Keeps a ref count so pausing can be layered
//--------------------------------------------------------------------------------------
void WINAPI DXUTPause( bool bPauseTime, bool bPauseRendering )
{
    int nPauseTimeCount = g_pDXUTState.GetPauseTimeCount();
    if( bPauseTime ) nPauseTimeCount++;
    else
        nPauseTimeCount--;
    if( nPauseTimeCount < 0 ) nPauseTimeCount = 0;
    g_pDXUTState.SetPauseTimeCount( nPauseTimeCount );

    int nPauseRenderingCount = g_pDXUTState.GetPauseRenderingCount();
    if( bPauseRendering ) nPauseRenderingCount++;
    else
        nPauseRenderingCount--;
    if( nPauseRenderingCount < 0 ) nPauseRenderingCount = 0;
    g_pDXUTState.SetPauseRenderingCount( nPauseRenderingCount );

    g_pDXUTState.SetRenderingPaused( nPauseRenderingCount > 0 );
    g_pDXUTState.SetTimePaused( nPauseTimeCount > 0 );
}


//--------------------------------------------------------------------------------------
// Starts a user defined timer callback
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTSetTimer( LPDXUTCALLBACKTIMER pCallbackTimer, float fTimeoutInSecs, UINT* pnIDEvent,
                             void* pCallbackUserContext )
{
    if( pCallbackTimer == NULL )
        return E_INVALIDARG;

    HRESULT hr;
    DXUT_TIMER DXUTTimer;
    DXUTTimer.pCallbackTimer = pCallbackTimer;
    DXUTTimer.pCallbackUserContext = pCallbackUserContext;
    DXUTTimer.fTimeoutInSecs = fTimeoutInSecs;
    DXUTTimer.fCountdown = fTimeoutInSecs;
    DXUTTimer.bEnabled = true;
    DXUTTimer.nID = g_pDXUTState.GetTimerLastID() + 1;
    g_pDXUTState.SetTimerLastID( DXUTTimer.nID );

    CGrowableArray <DXUT_TIMER>* pTimerList = g_pDXUTState.GetTimerList();
    if( pTimerList == NULL )
    {
        pTimerList = new CGrowableArray <DXUT_TIMER>;
        if( pTimerList == NULL )
            return E_OUTOFMEMORY;
        g_pDXUTState.SetTimerList( pTimerList );
    }

    if( FAILED( hr = pTimerList->Add( DXUTTimer ) ) )
        return hr;

    if( pnIDEvent )
        *pnIDEvent = DXUTTimer.nID;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Stops a user defined timer callback
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTKillTimer( UINT nIDEvent )
{
    CGrowableArray <DXUT_TIMER>* pTimerList = g_pDXUTState.GetTimerList();
    if( pTimerList == NULL )
        return S_FALSE;

    bool bFound = false;

    for( int i = 0; i < pTimerList->GetSize(); i++ )
    {
        DXUT_TIMER DXUTTimer = pTimerList->GetAt( i );
        if( DXUTTimer.nID == nIDEvent )
        {
            DXUTTimer.bEnabled = false;
            pTimerList->SetAt( i, DXUTTimer );
            bFound = true;
            break;
        }
    }

    if( !bFound )
        return E_INVALIDARG;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Internal helper function to handle calling the user defined timer callbacks
//--------------------------------------------------------------------------------------
void DXUTHandleTimers()
{
    float fElapsedTime = DXUTGetElapsedTime();

    CGrowableArray <DXUT_TIMER>* pTimerList = g_pDXUTState.GetTimerList();
    if( pTimerList == NULL )
        return;

    // Walk through the list of timer callbacks
    for( int i = 0; i < pTimerList->GetSize(); i++ )
    {
        DXUT_TIMER DXUTTimer = pTimerList->GetAt( i );
        if( DXUTTimer.bEnabled )
        {
            DXUTTimer.fCountdown -= fElapsedTime;

            // Call the callback if count down expired
            if( DXUTTimer.fCountdown < 0 )
            {
                DXUTTimer.pCallbackTimer( DXUTTimer.nID, DXUTTimer.pCallbackUserContext );
                // The callback my have changed the timer.
                DXUTTimer = pTimerList->GetAt( i );
                DXUTTimer.fCountdown = DXUTTimer.fTimeoutInSecs;
            }
            pTimerList->SetAt( i, DXUTTimer );
        }
    }
}


//--------------------------------------------------------------------------------------
// Display an custom error msg box 
//--------------------------------------------------------------------------------------
void DXUTDisplayErrorMessage( HRESULT hr )
{
    WCHAR strBuffer[512];

    int nExitCode;
    bool bFound = true;
    switch( hr )
    {
        case DXUTERR_NODIRECT3D:
        {
            nExitCode = 2;
         break;
        }
        case DXUTERR_NOCOMPATIBLEDEVICES:    
            nExitCode = 3; 
            if( GetSystemMetrics(0x1000) != 0 ) // SM_REMOTESESSION
                wcscpy_s( strBuffer, ARRAYSIZE(strBuffer), L"Direct3D does not work over a remote session." ); 
            else
                wcscpy_s( strBuffer, ARRAYSIZE(strBuffer), L"Could not find any compatible Direct3D devices." ); 
            break;
        case DXUTERR_MEDIANOTFOUND:          nExitCode = 4; wcscpy_s( strBuffer, ARRAYSIZE(strBuffer), L"Could not find required media." ); break;
        case DXUTERR_NONZEROREFCOUNT:        nExitCode = 5; wcscpy_s( strBuffer, ARRAYSIZE(strBuffer), L"The Direct3D device has a non-zero reference count, meaning some objects were not released." ); break;
        case DXUTERR_CREATINGDEVICE:         nExitCode = 6; wcscpy_s( strBuffer, ARRAYSIZE(strBuffer), L"Failed creating the Direct3D device." ); break;
        case DXUTERR_RESETTINGDEVICE:        nExitCode = 7; wcscpy_s( strBuffer, ARRAYSIZE(strBuffer), L"Failed resetting the Direct3D device." ); break;
        case DXUTERR_CREATINGDEVICEOBJECTS:  nExitCode = 8; wcscpy_s( strBuffer, ARRAYSIZE(strBuffer), L"An error occurred in the device create callback function." ); break;
        case DXUTERR_RESETTINGDEVICEOBJECTS: nExitCode = 9; wcscpy_s( strBuffer, ARRAYSIZE(strBuffer), L"An error occurred in the device reset callback function." ); break;
        // nExitCode 10 means the app exited using a REF device 
        case DXUTERR_DEVICEREMOVED:          nExitCode = 11; wcscpy_s( strBuffer, ARRAYSIZE(strBuffer), L"The Direct3D device was removed."  ); break;
        default: bFound = false; nExitCode = 1; break; // nExitCode 1 means the API was incorrectly called

    }   

    g_pDXUTState.SetExitCode(nExitCode);

    bool bShowMsgBoxOnError = g_pDXUTState.GetShowMsgBoxOnError();
    if( bFound && bShowMsgBoxOnError )
    {
        if( DXUTGetWindowTitle()[0] == 0 )
            MessageBox( DXUTGetHWND(), strBuffer, L"DXUT Application", MB_ICONERROR | MB_OK );
        else
            MessageBox( DXUTGetHWND(), strBuffer, DXUTGetWindowTitle(), MB_ICONERROR | MB_OK );
    }
}


//--------------------------------------------------------------------------------------
// Internal function to map MK_* to an array index
//--------------------------------------------------------------------------------------
int DXUTMapButtonToArrayIndex( BYTE vButton )
{
    switch( vButton )
    {
        case MK_LBUTTON:
            return 0;
        case VK_MBUTTON:
        case MK_MBUTTON:
            return 1;
        case MK_RBUTTON:
            return 2;
        case VK_XBUTTON1:
        case MK_XBUTTON1:
            return 3;
        case VK_XBUTTON2:
        case MK_XBUTTON2:
            return 4;
    }

    return 0;
}



//--------------------------------------------------------------------------------------
// Toggle between full screen and windowed
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTToggleFullScreen()
{
    HRESULT hr;
    DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();
    DXUTDeviceSettings orginalDeviceSettings = DXUTGetDeviceSettings();
    
    if (deviceSettings.ver == DXUT_D3D11_DEVICE) {
        deviceSettings.d3d11.sd.Windowed = !deviceSettings.d3d11.sd.Windowed; // datut
        if (!deviceSettings.d3d11.sd.Windowed) {
            DXGI_MODE_DESC adapterDesktopDisplayMode =
            {
                800, 600, { 60, 1 }, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            };
            DXUTGetD3D11AdapterDisplayMode( deviceSettings.d3d11.AdapterOrdinal, 0, &adapterDesktopDisplayMode );
            
            
            deviceSettings.d3d11.sd.BufferDesc = adapterDesktopDisplayMode;
        }else {
            RECT r = DXUTGetWindowClientRectAtModeChange();
            deviceSettings.d3d11.sd.BufferDesc.Height = r.bottom;
            deviceSettings.d3d11.sd.BufferDesc.Width = r.right;
        }
    }

    hr = DXUTChangeDevice( &deviceSettings, NULL, NULL,  false, false );

    // If hr == E_ABORT, this means the app rejected the device settings in the ModifySettingsCallback so nothing changed
    if( FAILED( hr ) && ( hr != E_ABORT ) )
    {
        // Failed creating device, try to switch back.
        HRESULT hr2 = DXUTChangeDevice( &orginalDeviceSettings, NULL,  NULL, false, false );
        if( FAILED( hr2 ) )
        {
            // If this failed, then shutdown
            DXUTShutdown();
        }
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Toggle between HAL and WARP
//--------------------------------------------------------------------------------------

HRESULT WINAPI DXUTToggleWARP () {
    HRESULT hr;

    DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();
    DXUTDeviceSettings orginalDeviceSettings = DXUTGetDeviceSettings();

    // Toggle between REF & HAL
    {
        ID3D11SwitchToRef* pD3D11STR = NULL;
        ID3D11Device* pDev = DXUTGetD3D11Device();
        assert( pDev != NULL );
        hr = pDev->QueryInterface( __uuidof( *pD3D11STR ), ( LPVOID* )&pD3D11STR );
        if( SUCCEEDED( hr ) )
        {
            pD3D11STR->SetUseRef( pD3D11STR->GetUseRef() ? FALSE : TRUE );
            SAFE_RELEASE( pD3D11STR );
            return S_OK;
        }

        if( deviceSettings.d3d11.DriverType == D3D_DRIVER_TYPE_HARDWARE || deviceSettings.d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE  )
            deviceSettings.d3d11.DriverType = D3D_DRIVER_TYPE_WARP;
        else if( deviceSettings.d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE || deviceSettings.d3d11.DriverType == D3D_DRIVER_TYPE_WARP )
            deviceSettings.d3d11.DriverType = D3D_DRIVER_TYPE_HARDWARE;
    }
    
    hr = DXUTSnapDeviceSettingsToEnumDevice(&deviceSettings, false);
    if( SUCCEEDED( hr ) )
    {
        // Create a Direct3D device using the new device settings.  
        // If there is an existing device, then it will either reset or recreate the scene.
        hr = DXUTChangeDevice( &deviceSettings, NULL,  NULL, false, false );

        // If hr == E_ABORT, this means the app rejected the device settings in the ModifySettingsCallback so nothing changed
        if( FAILED( hr ) && ( hr != E_ABORT ) )
        {
            // Failed creating device, try to switch back.
            HRESULT hr2 = DXUTChangeDevice( &orginalDeviceSettings, NULL, NULL, false, false );
            if( FAILED( hr2 ) )
            {
                // If this failed, then shutdown
                DXUTShutdown();
            }
        }
    }

    return hr;
}
//--------------------------------------------------------------------------------------
// Toggle between HAL and REF
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTToggleREF()
{
    HRESULT hr;

    DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();
    DXUTDeviceSettings orginalDeviceSettings = DXUTGetDeviceSettings();

    // Toggle between REF & HAL
    {
        ID3D11SwitchToRef* pD3D11STR = NULL;
        ID3D11Device* pDev = DXUTGetD3D11Device();
        assert( pDev != NULL );
        hr = pDev->QueryInterface( __uuidof( *pD3D11STR ), ( LPVOID* )&pD3D11STR );
        if( SUCCEEDED( hr ) )
        {
            pD3D11STR->SetUseRef( pD3D11STR->GetUseRef() ? FALSE : TRUE );
            SAFE_RELEASE( pD3D11STR );
            return S_OK;
        }

        if( deviceSettings.d3d11.DriverType == D3D_DRIVER_TYPE_HARDWARE )
            deviceSettings.d3d11.DriverType = D3D_DRIVER_TYPE_REFERENCE;
        else if( deviceSettings.d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE )
            deviceSettings.d3d11.DriverType = D3D_DRIVER_TYPE_HARDWARE;
    }
    
    hr = DXUTSnapDeviceSettingsToEnumDevice(&deviceSettings, false);
    if( SUCCEEDED( hr ) )
    {
        // Create a Direct3D device using the new device settings.  
        // If there is an existing device, then it will either reset or recreate the scene.
        hr = DXUTChangeDevice( &deviceSettings, NULL,  NULL, false, false );

        // If hr == E_ABORT, this means the app rejected the device settings in the ModifySettingsCallback so nothing changed
        if( FAILED( hr ) && ( hr != E_ABORT ) )
        {
            // Failed creating device, try to switch back.
            HRESULT hr2 = DXUTChangeDevice( &orginalDeviceSettings, NULL, NULL, false, false );
            if( FAILED( hr2 ) )
            {
                // If this failed, then shutdown
                DXUTShutdown();
            }
        }
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// Checks to see if DXGI has switched us out of fullscreen or windowed mode
//--------------------------------------------------------------------------------------
void DXUTCheckForDXGIFullScreenSwitch()
{
    DXUTDeviceSettings* pDeviceSettings = g_pDXUTState.GetCurrentDeviceSettings();
    {
        IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();
        assert( pSwapChain != NULL );
        DXGI_SWAP_CHAIN_DESC SCDesc;
        pSwapChain->GetDesc( &SCDesc );

        BOOL bIsWindowed = ( BOOL )DXUTIsWindowed();
        if( bIsWindowed != SCDesc.Windowed )
        {
            pDeviceSettings->d3d11.sd.Windowed = SCDesc.Windowed;

            DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();

            if( bIsWindowed )
            {
                g_pDXUTState.SetWindowBackBufferWidthAtModeChange( deviceSettings.d3d11.sd.BufferDesc.Width );
                g_pDXUTState.SetWindowBackBufferHeightAtModeChange( deviceSettings.d3d11.sd.BufferDesc.Height );
            }
            else
            {
                g_pDXUTState.SetFullScreenBackBufferWidthAtModeChange( deviceSettings.d3d11.sd.BufferDesc.Width );
                g_pDXUTState.SetFullScreenBackBufferHeightAtModeChange( deviceSettings.d3d11.sd.BufferDesc.Height );
            }
        }
    }
}

void DXUTResizeDXGIBuffers( UINT Width, UINT Height, BOOL bFullScreen )
{
	//return;
    HRESULT hr = S_OK;
    RECT rcCurrentClient;
    GetClientRect( DXUTGetHWND(), &rcCurrentClient );

    DXUTDeviceSettings* pDevSettings = g_pDXUTState.GetCurrentDeviceSettings();
    assert( pDevSettings != NULL );

    IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();

    ID3D11Device* pd3dDevice = DXUTGetD3D11Device();
    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

    // Determine if we're fullscreen
    pDevSettings->d3d11.sd.Windowed = !bFullScreen;

    // Call releasing
    g_pDXUTState.SetInsideDeviceCallback( true );
    LPDXUTCALLBACKD3D11SWAPCHAINRELEASING pCallbackSwapChainReleasing = g_pDXUTState.GetD3D11SwapChainReleasingFunc
        ();
    if( pCallbackSwapChainReleasing != NULL )
        pCallbackSwapChainReleasing( g_pDXUTState.GetD3D11SwapChainResizedFuncUserContext() );
    g_pDXUTState.SetInsideDeviceCallback( false );

    // Release our old depth stencil texture and view 
    ID3D11Texture2D* pDS = g_pDXUTState.GetD3D11DepthStencil();
    SAFE_RELEASE( pDS );
    g_pDXUTState.SetD3D11DepthStencil( NULL );
    ID3D11DepthStencilView* pDSV = g_pDXUTState.GetD3D11DepthStencilView();
    SAFE_RELEASE( pDSV );
    g_pDXUTState.SetD3D11DepthStencilView( NULL );

    // Release our old render target view
    ID3D11RenderTargetView* pRTV = g_pDXUTState.GetD3D11RenderTargetView();
    SAFE_RELEASE( pRTV );
    g_pDXUTState.SetD3D11RenderTargetView( NULL );

    // Alternate between 0 and DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH when resizing buffers.
    // When in windowed mode, we want 0 since this allows the app to change to the desktop
    // resolution from windowed mode during alt+enter.  However, in fullscreen mode, we want
    // the ability to change display modes from the Device Settings dialog.  Therefore, we
    // want to set the DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH flag.
    UINT Flags = 0;
    if( bFullScreen )
        Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    // ResizeBuffers
    pSwapChain->ResizeBuffers( pDevSettings->d3d11.sd.BufferCount,
                                  Width,
                                  Height,
                                  pDevSettings->d3d11.sd.BufferDesc.Format,
                                  Flags ) ;

    if( !g_pDXUTState.GetDoNotStoreBufferSize() )
    {
        pDevSettings->d3d11.sd.BufferDesc.Width = ( UINT )rcCurrentClient.right;
        pDevSettings->d3d11.sd.BufferDesc.Height = ( UINT )rcCurrentClient.bottom;
    }

    // Save off backbuffer desc
    DXUTUpdateBackBufferDesc();

    // Update the device stats text

    // Setup the render target view and viewport
    hr = DXUTCreateD3D11Views( pd3dDevice, pd3dImmediateContext, pDevSettings );
    if( FAILED( hr ) )
    {
        return;
    }

    // Setup cursor based on current settings (window/fullscreen mode, show cursor state, clip cursor state)
    DXUTSetupCursor();

    // Call the app's SwapChain reset callback
    g_pDXUTState.SetInsideDeviceCallback( true );
    const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc = DXUTGetDXGIBackBufferSurfaceDesc();
    LPDXUTCALLBACKD3D11SWAPCHAINRESIZED pCallbackSwapChainResized = g_pDXUTState.GetD3D11SwapChainResizedFunc();
    hr = S_OK;
    if( pCallbackSwapChainResized != NULL )
        hr = pCallbackSwapChainResized( pd3dDevice, pSwapChain, pBackBufferSurfaceDesc,
                                        g_pDXUTState.GetD3D11SwapChainResizedFuncUserContext() );
    g_pDXUTState.SetInsideDeviceCallback( false );
    if( FAILED( hr ) )
    {
        // If callback failed, cleanup
        if( hr != DXUTERR_MEDIANOTFOUND )
            hr = DXUTERR_RESETTINGDEVICEOBJECTS;

        g_pDXUTState.SetInsideDeviceCallback( true );
        LPDXUTCALLBACKD3D11SWAPCHAINRELEASING pCallbackSwapChainReleasing =
            g_pDXUTState.GetD3D11SwapChainReleasingFunc();
        if( pCallbackSwapChainReleasing != NULL )
            pCallbackSwapChainReleasing( g_pDXUTState.GetD3D11SwapChainResizedFuncUserContext() );
        g_pDXUTState.SetInsideDeviceCallback( false );
        DXUTPause( false, false );
        PostQuitMessage( 0 );
    }
    else
    {
        g_pDXUTState.SetDeviceObjectsReset( true );
        DXUTPause( false, false );
    }
}

//--------------------------------------------------------------------------------------
// Checks if DXGI buffers need to change
//--------------------------------------------------------------------------------------
void DXUTCheckForDXGIBufferChange()
{
    if(DXUTGetDXGISwapChain() != NULL && !g_pDXUTState.GetReleasingSwapChain() )
    {
        //DXUTgetdxgi
        IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();

        // Determine if we're fullscreen
        BOOL bFullScreen;
        pSwapChain->GetFullscreenState( &bFullScreen, NULL );

        DXUTResizeDXGIBuffers( 0, 0, bFullScreen );

        ShowWindow( DXUTGetHWND(), SW_SHOW );
    }
}

//--------------------------------------------------------------------------------------
// Checks if the window client rect has changed and if it has, then reset the device
//--------------------------------------------------------------------------------------
void DXUTCheckForWindowSizeChange()
{
    // Skip the check for various reasons
          
    if( g_pDXUTState.GetIgnoreSizeChange() || !g_pDXUTState.GetDeviceCreated() )
        return;
	DXUTCheckForDXGIBufferChange();
}

//--------------------------------------------------------------------------------------
// Renders the scene using either D3D9 or D3D11
//--------------------------------------------------------------------------------------
void WINAPI DXUTRender3DEnvironment()
{
    DXUTRender3DEnvironment11();
}

//--------------------------------------------------------------------------------------
// Returns the HMONITOR attached to an adapter/output
//--------------------------------------------------------------------------------------
HMONITOR DXUTGetMonitorFromAdapter(  )
{
        CD3D11Enumeration* pD3DEnum = DXUTGetD3D11Enumeration();
        assert( pD3DEnum != NULL );
        CD3D11EnumOutputInfo* pOutputInfo = pD3DEnum->GetOutputInfo( 0,
                                                                     0 );
        if( !pOutputInfo )
            return 0;
        return DXUTMonitorFromRect( &pOutputInfo->Desc.DesktopCoordinates, MONITOR_DEFAULTTONEAREST );
}


//--------------------------------------------------------------------------------------
// Look for an adapter ordinal that is tied to a HMONITOR
//--------------------------------------------------------------------------------------
HRESULT DXUTGetAdapterOrdinalFromMonitor( HMONITOR hMonitor, UINT* pAdapterOrdinal )
{
    *pAdapterOrdinal = 0;
    // Get the monitor handle information
    MONITORINFOEX mi;
    mi.cbSize = sizeof( MONITORINFOEX );
    DXUTGetMonitorInfo( hMonitor, &mi );

    // Search for this monitor in our enumeration hierarchy.
    CD3D11Enumeration* pd3dEnum = DXUTGetD3D11Enumeration();
    CGrowableArray <CD3D11EnumAdapterInfo*>* pAdapterList = pd3dEnum->GetAdapterInfoList();
    for( int iAdapter = 0; iAdapter < pAdapterList->GetSize(); ++iAdapter )
    {
        CD3D11EnumAdapterInfo* pAdapterInfo = pAdapterList->GetAt( iAdapter );
        for( int o = 0; o < pAdapterInfo->outputInfoList.GetSize(); ++o )
        {
            CD3D11EnumOutputInfo* pOutputInfo = pAdapterInfo->outputInfoList.GetAt( o );
            // Convert output device name from MBCS to Unicode
            if( wcsncmp( pOutputInfo->Desc.DeviceName, mi.szDevice, sizeof( mi.szDevice ) / sizeof
                            ( mi.szDevice[0] ) ) == 0 )
            {
                *pAdapterOrdinal = pAdapterInfo->AdapterOrdinal;
                return S_OK;
            }
        }
    }

    return E_FAIL;
}

//--------------------------------------------------------------------------------------
// Look for a monitor ordinal that is tied to a HMONITOR (D3D11-only)
//--------------------------------------------------------------------------------------
HRESULT DXUTGetOutputOrdinalFromMonitor( HMONITOR hMonitor, UINT* pOutputOrdinal )
{
    // Get the monitor handle information
    MONITORINFOEX mi;
    mi.cbSize = sizeof( MONITORINFOEX );
    DXUTGetMonitorInfo( hMonitor, &mi );

    // Search for this monitor in our enumeration hierarchy.
    CD3D11Enumeration* pd3dEnum = DXUTGetD3D11Enumeration();
    CGrowableArray <CD3D11EnumAdapterInfo*>* pAdapterList = pd3dEnum->GetAdapterInfoList();
    for( int iAdapter = 0; iAdapter < pAdapterList->GetSize(); ++iAdapter )
    {
        CD3D11EnumAdapterInfo* pAdapterInfo = pAdapterList->GetAt( iAdapter );
        for( int o = 0; o < pAdapterInfo->outputInfoList.GetSize(); ++o )
        {
            CD3D11EnumOutputInfo* pOutputInfo = pAdapterInfo->outputInfoList.GetAt( o );
            DXGI_OUTPUT_DESC Desc;
            pOutputInfo->m_pOutput->GetDesc( &Desc );

            if( hMonitor == Desc.Monitor )
            {
                *pOutputOrdinal = pOutputInfo->Output;
                return S_OK;
            }
        }
    }

    return E_FAIL;
}

//--------------------------------------------------------------------------------------
// This method is called when D3DERR_DEVICEREMOVED is returned from an API.  DXUT
// calls the application's DeviceRemoved callback to inform it of the event.  The
// application returns true if it wants DXUT to look for a closest device to run on.
// If no device is found, or the app returns false, DXUT shuts down.
//--------------------------------------------------------------------------------------
HRESULT DXUTHandleDeviceRemoved()
{
    HRESULT hr = S_OK;

    // Device has been removed. Call the application's callback if set.  If no callback
    // has been set, then just look for a new device
    bool bLookForNewDevice = true;
    LPDXUTCALLBACKDEVICEREMOVED pDeviceRemovedFunc = g_pDXUTState.GetDeviceRemovedFunc();
    if( pDeviceRemovedFunc )
        bLookForNewDevice = pDeviceRemovedFunc( g_pDXUTState.GetDeviceRemovedFuncUserContext() );

    if( bLookForNewDevice )
    {
        DXUTDeviceSettings* pDeviceSettings = g_pDXUTState.GetCurrentDeviceSettings();


        hr = DXUTSnapDeviceSettingsToEnumDevice( pDeviceSettings, false);
        if( SUCCEEDED( hr ) )
        {
            // Change to a Direct3D device created from the new device settings
            // that is compatible with the removed device.
            hr = DXUTChangeDevice( pDeviceSettings, NULL, NULL, true, false );
            if( SUCCEEDED( hr ) )
                return S_OK;
        }
    }

    // The app does not wish to continue or continuing is not possible.
    return DXUTERR_DEVICEREMOVED;
}


//--------------------------------------------------------------------------------------
// Stores back buffer surface desc in g_pDXUTState.GetBackBufferSurfaceDesc10()
//--------------------------------------------------------------------------------------
void DXUTUpdateBackBufferDesc()
{
    {
        HRESULT hr;
        ID3D11Texture2D* pBackBuffer;
        IDXGISwapChain* pSwapChain = g_pDXUTState.GetDXGISwapChain();
        assert( pSwapChain != NULL );
        hr = pSwapChain->GetBuffer( 0, __uuidof( *pBackBuffer ), ( LPVOID* )&pBackBuffer );
        DXGI_SURFACE_DESC* pBBufferSurfaceDesc = g_pDXUTState.GetBackBufferSurfaceDescDXGI();
        ZeroMemory( pBBufferSurfaceDesc, sizeof( DXGI_SURFACE_DESC ) );
        if( SUCCEEDED( hr ) )
        {
            D3D11_TEXTURE2D_DESC TexDesc;
            pBackBuffer->GetDesc( &TexDesc );
            pBBufferSurfaceDesc->Width = ( UINT )TexDesc.Width;
            pBBufferSurfaceDesc->Height = ( UINT )TexDesc.Height;
            pBBufferSurfaceDesc->Format = TexDesc.Format;
            pBBufferSurfaceDesc->SampleDesc = TexDesc.SampleDesc;
            SAFE_RELEASE( pBackBuffer );
        }
    }
}


//--------------------------------------------------------------------------------------
// Setup cursor based on current settings (window/fullscreen mode, show cursor state, clip cursor state)
//--------------------------------------------------------------------------------------
void DXUTSetupCursor()
{
    {
        // Clip cursor if requested
        if( !DXUTIsWindowed() && g_pDXUTState.GetClipCursorWhenFullScreen() )
        {
            // Confine cursor to full screen window
            RECT rcWindow;
            GetWindowRect( DXUTGetHWNDDeviceFullScreen(), &rcWindow );
            ClipCursor( &rcWindow );
        }
        else
        {
            ClipCursor( NULL );
        }
    }
}

//--------------------------------------------------------------------------------------
// Updates the frames/sec stat once per second
//--------------------------------------------------------------------------------------
void DXUTUpdateFrameStats()
{
    if( g_pDXUTState.GetNoStats() )
        return;

    // Keep track of the frame count
    double fLastTime = g_pDXUTState.GetLastStatsUpdateTime();
    DWORD dwFrames = g_pDXUTState.GetLastStatsUpdateFrames();
    double fAbsTime = g_pDXUTState.GetAbsoluteTime();
    dwFrames++;
    g_pDXUTState.SetLastStatsUpdateFrames( dwFrames );

    // Update the scene stats once per second
    if( fAbsTime - fLastTime > 1.0f )
    {
        float fFPS = ( float )( dwFrames / ( fAbsTime - fLastTime ) );
        g_pDXUTState.SetFPS( fFPS );
        g_pDXUTState.SetLastStatsUpdateTime( fAbsTime );
        g_pDXUTState.SetLastStatsUpdateFrames( 0 );

        WCHAR* pstrFPS = g_pDXUTState.GetFPSStats();
        swprintf_s( pstrFPS, 64, L"%0.2f fps ", fFPS );
    }
}

//--------------------------------------------------------------------------------------
// Returns a string describing the current device.  If bShowFPS is true, then
// the string contains the frames/sec.  If "-nostats" was used in 
// the command line, the string will be blank
//--------------------------------------------------------------------------------------
LPCWSTR WINAPI DXUTGetFrameStats( bool bShowFPS )
{
    WCHAR* pstrFrameStats = g_pDXUTState.GetFrameStats();
    WCHAR* pstrFPS = ( bShowFPS ) ? g_pDXUTState.GetFPSStats() : L"";
    swprintf_s( pstrFrameStats, 256, g_pDXUTState.GetStaticFrameStats(), pstrFPS );
    return pstrFrameStats;
}

//--------------------------------------------------------------------------------------
// Updates the string which describes the device 
//--------------------------------------------------------------------------------------
void DXUTUpdateD3D11DeviceStats( D3D_DRIVER_TYPE DeviceType, DXGI_ADAPTER_DESC* pAdapterDesc )
{
    if( g_pDXUTState.GetNoStats() )
        return;

    // Store device description
    WCHAR* pstrDeviceStats = g_pDXUTState.GetDeviceStats();
    if( DeviceType == D3D_DRIVER_TYPE_REFERENCE )
        wcscpy_s( pstrDeviceStats, 256, L"REFERENCE" );
    else if( DeviceType == D3D_DRIVER_TYPE_HARDWARE )
        wcscpy_s( pstrDeviceStats, 256, L"HARDWARE" );
    else if( DeviceType == D3D_DRIVER_TYPE_SOFTWARE )
        wcscpy_s( pstrDeviceStats, 256, L"SOFTWARE" );
    else if( DeviceType == D3D_DRIVER_TYPE_WARP )
        wcscpy_s( pstrDeviceStats, 256, L"WARP" );

    if( DeviceType == D3D_DRIVER_TYPE_HARDWARE )
    {
        // Be sure not to overflow m_strDeviceStats when appending the adapter 
        // description, since it can be long.  
        wcscat_s( pstrDeviceStats, 256, L": " );

        // Try to get a unique description from the CD3D11EnumDeviceSettingsCombo
        DXUTDeviceSettings* pDeviceSettings = g_pDXUTState.GetCurrentDeviceSettings();
        if( !pDeviceSettings )
            return;

        CD3D11Enumeration* pd3dEnum = DXUTGetD3D11Enumeration();
        assert( pd3dEnum != NULL );
        CD3D11EnumDeviceSettingsCombo* pDeviceSettingsCombo = pd3dEnum->GetDeviceSettingsCombo(
            pDeviceSettings->d3d11.AdapterOrdinal, pDeviceSettings->d3d11.DriverType, pDeviceSettings->d3d11.Output,
            pDeviceSettings->d3d11.sd.BufferDesc.Format, pDeviceSettings->d3d11.sd.Windowed );
        if( pDeviceSettingsCombo )
            wcscat_s( pstrDeviceStats, 256, pDeviceSettingsCombo->pAdapterInfo->szUniqueDescription );
        else
            wcscat_s( pstrDeviceStats, 256, pAdapterDesc->Description );
    }
}


//--------------------------------------------------------------------------------------
// Misc functions
//--------------------------------------------------------------------------------------
DXUTDeviceSettings WINAPI DXUTGetDeviceSettings()
{
    // Return a copy of device settings of the current device.  If no device exists yet, then
    // return a blank device settings struct
    DXUTDeviceSettings* pDS = g_pDXUTState.GetCurrentDeviceSettings();
    if( pDS )
    {
        return *pDS;
    }
    else
    {
        DXUTDeviceSettings ds;
        ZeroMemory( &ds, sizeof( DXUTDeviceSettings ) );
        return ds;
    }
}

bool WINAPI DXUTIsVsyncEnabled()
{
    DXUTDeviceSettings* pDS = g_pDXUTState.GetCurrentDeviceSettings();
    if( pDS )
    {
        return ( pDS->d3d11.SyncInterval == 0 );
    }
    else
    {
        return true;
    }
};

bool WINAPI DXUTIsKeyDown( BYTE vKey )
{
    bool* bKeys = g_pDXUTState.GetKeys();
    if( vKey >= 0xA0 && vKey <= 0xA5 )  // VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU
        return GetAsyncKeyState( vKey ) != 0; // these keys only are tracked via GetAsyncKeyState()
    else if( vKey >= 0x01 && vKey <= 0x06 && vKey != 0x03 ) // mouse buttons (VK_*BUTTON)
        return DXUTIsMouseButtonDown( vKey );
    else
        return bKeys[vKey];
}

bool WINAPI DXUTWasKeyPressed( BYTE vKey )
{
    bool* bLastKeys = g_pDXUTState.GetLastKeys();
    bool* bKeys = g_pDXUTState.GetKeys();
    g_pDXUTState.SetAppCalledWasKeyPressed( true );
    return ( !bLastKeys[vKey] && bKeys[vKey] );
}

bool WINAPI DXUTIsMouseButtonDown( BYTE vButton )
{
    bool* bMouseButtons = g_pDXUTState.GetMouseButtons();
    int nIndex = DXUTMapButtonToArrayIndex( vButton );
    return bMouseButtons[nIndex];
}

void WINAPI DXUTSetMultimonSettings( bool bAutoChangeAdapter )
{
    g_pDXUTState.SetAutoChangeAdapter( bAutoChangeAdapter );
}

void WINAPI DXUTSetHotkeyHandling( bool bAltEnterToToggleFullscreen, bool bEscapeToQuit, bool bPauseToToggleTimePause )
{
    g_pDXUTState.SetHandleEscape( bEscapeToQuit );
    g_pDXUTState.SetHandleAltEnter( bAltEnterToToggleFullscreen );
    g_pDXUTState.SetHandlePause( bPauseToToggleTimePause );
}

void WINAPI DXUTSetCursorSettings( bool bShowCursorWhenFullScreen, bool bClipCursorWhenFullScreen )
{
    g_pDXUTState.SetClipCursorWhenFullScreen( bClipCursorWhenFullScreen );
    g_pDXUTState.SetShowCursorWhenFullScreen( bShowCursorWhenFullScreen );
    DXUTSetupCursor();
}

void WINAPI DXUTSetWindowSettings( bool bCallDefWindowProc )
{
    g_pDXUTState.SetCallDefWindowProc( bCallDefWindowProc );
}

void WINAPI DXUTSetConstantFrameTime( bool bEnabled, float fTimePerFrame )
{
    if( g_pDXUTState.GetOverrideConstantFrameTime() )
    {
        bEnabled = g_pDXUTState.GetOverrideConstantFrameTime();
        fTimePerFrame = g_pDXUTState.GetOverrideConstantTimePerFrame();
    }
    g_pDXUTState.SetConstantFrameTime( bEnabled );
    g_pDXUTState.SetTimePerFrame( fTimePerFrame );
}


//--------------------------------------------------------------------------------------
// Resets the state associated with DXUT 
//--------------------------------------------------------------------------------------
void WINAPI DXUTResetFrameworkState()
{
    g_pDXUTState.Destroy();
    g_pDXUTState.Create();
}


//--------------------------------------------------------------------------------------
// Closes down the window.  When the window closes, it will cleanup everything
//--------------------------------------------------------------------------------------
void WINAPI DXUTShutdown( int nExitCode )
{
    HWND hWnd = DXUTGetHWND();
    if( hWnd != NULL )
        SendMessage( hWnd, WM_CLOSE, 0, 0 );

    g_pDXUTState.SetExitCode( nExitCode );

    // Restore shortcut keys (Windows key, accessibility shortcuts) to original state
    // This is important to call here if the shortcuts are disabled, 
    // because accessibility setting changes are permanent.
    // This means that if this is not done then the accessibility settings 
    // might not be the same as when the app was started. 
    // If the app crashes without restoring the settings, this is also true so it
    // would be wise to backup/restore the settings from a file so they can be 
    // restored when the crashed app is run again.

    // Shutdown D3D11
    IDXGIFactory1* pDXGIFactory = g_pDXUTState.GetDXGIFactory();
    SAFE_RELEASE( pDXGIFactory );
    g_pDXUTState.SetDXGIFactory( NULL );
}

void DXUTApplyDefaultDeviceSettings(DXUTDeviceSettings *modifySettings) {
    ZeroMemory( modifySettings, sizeof( DXUTDeviceSettings ) );


    modifySettings->ver = DXUT_D3D11_DEVICE;
    modifySettings->d3d11.AdapterOrdinal = 0;
    modifySettings->d3d11.AutoCreateDepthStencil = true;
    modifySettings->d3d11.AutoDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
#if defined(DEBUG) || defined(_DEBUG)
    modifySettings->d3d11.CreateFlags |= D3D10_CREATE_DEVICE_DEBUG;
#else
    modifySettings->d3d11.CreateFlags = 0;
#endif
    modifySettings->d3d11.DriverType = D3D_DRIVER_TYPE_HARDWARE;
    modifySettings->d3d11.Output = 0;
    modifySettings->d3d11.PresentFlags = 0;
    modifySettings->d3d11.sd.BufferCount = 2;
    modifySettings->d3d11.sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    modifySettings->d3d11.sd.BufferDesc.Height = 480;
    modifySettings->d3d11.sd.BufferDesc.RefreshRate.Numerator = 60;
    modifySettings->d3d11.sd.BufferDesc.RefreshRate.Denominator = 1;
    modifySettings->d3d11.sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    modifySettings->d3d11.sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    modifySettings->d3d11.sd.BufferDesc.Width = 640;
    modifySettings->d3d11.sd.BufferUsage = 32;
    modifySettings->d3d11.sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH ;
    modifySettings->d3d11.sd.OutputWindow = DXUTGetHWND();
    modifySettings->d3d11.sd.SampleDesc.Count = 1;
    modifySettings->d3d11.sd.SampleDesc.Quality = 0;
    modifySettings->d3d11.sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    modifySettings->d3d11.sd.Windowed = 1;
    modifySettings->d3d11.SyncInterval = 0;
}



//--------------------------------------------------------------------------------------
// Update settings based on what is enumeratabled
//--------------------------------------------------------------------------------------
HRESULT DXUTSnapDeviceSettingsToEnumDevice( DXUTDeviceSettings* pDeviceSettings, bool forceEnum,  D3D_FEATURE_LEVEL forceFL )
{
    if( GetSystemMetrics(0x1000) != 0 ) 
	{// SM_REMOTESESSION
        pDeviceSettings->d3d11.sd.Windowed = 1;
    }   
    int bestModeIndex=0;
    int bestMSAAIndex=0;


    //DXUTSetDefaultDeviceSettings
    if (pDeviceSettings->ver == DXUT_D3D11_DEVICE )
	{
        CD3D11Enumeration *pEnum = NULL;

		// 枚举D3D11参数
        pEnum = DXUTGetD3D11Enumeration( forceEnum, false, forceFL);

        CD3D11EnumAdapterInfo* pAdapterInfo = NULL; 
        CGrowableArray <CD3D11EnumAdapterInfo*>* pAdapterList = pEnum->GetAdapterInfoList();
        CD3D11EnumAdapterInfo* tempAdapterInfo = pAdapterList->GetAt( 0 );
        for( int iAdapter = 0; iAdapter < pAdapterList->GetSize(); iAdapter++ )
        {
            tempAdapterInfo = pAdapterList->GetAt( iAdapter );
            if (tempAdapterInfo->AdapterOrdinal == pDeviceSettings->d3d11.AdapterOrdinal) pAdapterInfo = tempAdapterInfo;
        }
        if (pAdapterInfo == NULL) return E_FAIL; // no adapters found.
        CD3D11EnumDeviceSettingsCombo* pDeviceSettingsCombo = NULL;
        float biggestScore = 0;

        int combo = 0;
        for( int iDeviceCombo = 0; iDeviceCombo < pAdapterInfo->deviceSettingsComboList.GetSize(); iDeviceCombo++ )
        {
            CD3D11EnumDeviceSettingsCombo* tempDeviceSettingsCombo = pAdapterInfo->deviceSettingsComboList.GetAt( iDeviceCombo );
    
            DXGI_MODE_DESC adapterDisplayMode;
            DXUTGetD3D11AdapterDisplayMode( pAdapterInfo->AdapterOrdinal, 0, &adapterDisplayMode );

            int bestMode;
            int bestMSAA;
            float score = DXUTRankD3D11DeviceCombo(tempDeviceSettingsCombo, &(pDeviceSettings->d3d11), &adapterDisplayMode, bestMode, bestMSAA );
            if (score > biggestScore) {
                combo = iDeviceCombo;
                biggestScore = score;
                pDeviceSettingsCombo = tempDeviceSettingsCombo;
                bestModeIndex = bestMode;
                bestMSAAIndex = bestMSAA;
            }
               
        }
        if (NULL == pDeviceSettingsCombo ) {
            return E_FAIL; // no settigns found.
        }

        pDeviceSettings->d3d11.AdapterOrdinal = pDeviceSettingsCombo->AdapterOrdinal;
        pDeviceSettings->d3d11.DriverType = pDeviceSettingsCombo->DeviceType;
        pDeviceSettings->d3d11.Output = pDeviceSettingsCombo->Output;
    
        pDeviceSettings->d3d11.sd.Windowed = pDeviceSettingsCombo->Windowed;
        if( GetSystemMetrics(0x1000) != 0 ) {// SM_REMOTESESSION
            pDeviceSettings->d3d11.sd.Windowed = 1;
        }   
        if (pDeviceSettingsCombo->pOutputInfo != NULL) {
            DXGI_MODE_DESC bestDisplayMode;
            bestDisplayMode = pDeviceSettingsCombo->pOutputInfo->displayModeList.GetAt(bestModeIndex);
            if (!pDeviceSettingsCombo->Windowed) {

                pDeviceSettings->d3d11.sd.BufferDesc.Height = bestDisplayMode.Height;
                pDeviceSettings->d3d11.sd.BufferDesc.Width = bestDisplayMode.Width;
                pDeviceSettings->d3d11.sd.BufferDesc.RefreshRate.Numerator = bestDisplayMode.RefreshRate.Numerator;
                pDeviceSettings->d3d11.sd.BufferDesc.RefreshRate.Denominator = bestDisplayMode.RefreshRate.Denominator;
                pDeviceSettings->d3d11.sd.BufferDesc.Scaling = bestDisplayMode.Scaling;
                pDeviceSettings->d3d11.sd.BufferDesc.ScanlineOrdering = bestDisplayMode.ScanlineOrdering;
            }
        }
        if (pDeviceSettings->d3d11.DeviceFeatureLevel == 0)
            pDeviceSettings->d3d11.DeviceFeatureLevel = pDeviceSettingsCombo->pDeviceInfo->SelectedLevel;
        


        
        pDeviceSettings->d3d11.sd.SampleDesc.Count = pDeviceSettingsCombo->multiSampleCountList.GetAt(bestMSAAIndex);
        if (pDeviceSettings->d3d11.sd.SampleDesc.Quality > pDeviceSettingsCombo->multiSampleQualityList.GetAt(bestMSAAIndex) - 1)
            pDeviceSettings->d3d11.sd.SampleDesc.Quality = pDeviceSettingsCombo->multiSampleQualityList.GetAt(bestMSAAIndex) - 1;
        
        pDeviceSettings->d3d11.sd.BufferDesc.Format = pDeviceSettingsCombo->BackBufferFormat;

        return S_OK;
    }
    return E_FAIL;
}


//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTCreateGUITextureFromInternalArray11(ID3D11Device* pd3dDevice, ID3D11Texture2D** ppTexture, D3DX11_IMAGE_INFO* pInfo)
{
	HRESULT hr;

	ID3D11Resource *pRes;
	D3DX11_IMAGE_LOAD_INFO loadInfo;
	loadInfo.Width = D3DX11_DEFAULT;
	loadInfo.Height = D3DX11_DEFAULT;
	loadInfo.Depth = D3DX11_DEFAULT;
	loadInfo.FirstMipLevel = 0;
	loadInfo.MipLevels = 1;
	loadInfo.Usage = D3D11_USAGE_DEFAULT;
	loadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	loadInfo.CpuAccessFlags = 0;
	loadInfo.MiscFlags = 0;
	loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	loadInfo.Filter = D3DX11_FILTER_NONE;
	loadInfo.MipFilter = D3DX11_FILTER_NONE;
	loadInfo.pSrcInfo = pInfo;

	// 图片路径
	CString path;
	::GetModuleFileName(NULL, path.GetBufferSetLength(MAX_PATH), MAX_PATH);
	path = path.Left(path.ReverseFind(L'\\')) + L"\\aa.png";
	//
	hr = D3DX11CreateTextureFromFile(pd3dDevice, path, &loadInfo, NULL, &pRes, NULL);
	if (FAILED(hr))
		return hr;
	hr = pRes->QueryInterface(__uuidof(ID3D11Texture2D), (LPVOID*) ppTexture);
	SAFE_RELEASE(pRes);

	return S_OK;
}