//--------------------------------------------------------------------------------------
// File: DXUTgui.h
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <usp10.h>
#include <dimm.h>


//--------------------------------------------------------------------------------------
// Defines and macros 
//--------------------------------------------------------------------------------------
#define EVENT_BUTTON_CLICKED                0x0101

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
class CDXUTDialogResourceManager;
class CDXUTControl;
class CDXUTButton;
class CDXUTElement;
struct DXUTElementHolder;
struct DXUTTextureNode;
typedef VOID ( CALLBACK*PCALLBACKDXUTGUIEVENT )( UINT nEvent, int nControlID, CDXUTControl* pControl,
                                                 void* pUserContext );


//--------------------------------------------------------------------------------------
// Enums for pre-defined control types
//--------------------------------------------------------------------------------------
enum DXUT_CONTROL_TYPE
{
    DXUT_CONTROL_BUTTON,
    DXUT_CONTROL_STATIC,
    DXUT_CONTROL_CHECKBOX,
    DXUT_CONTROL_RADIOBUTTON,
    DXUT_CONTROL_COMBOBOX,
    DXUT_CONTROL_SLIDER,
    DXUT_CONTROL_EDITBOX,
    DXUT_CONTROL_IMEEDITBOX,
    DXUT_CONTROL_LISTBOX,
    DXUT_CONTROL_SCROLLBAR,
};

enum DXUT_CONTROL_STATE
{
    DXUT_STATE_NORMAL = 0,
    DXUT_STATE_DISABLED,
    DXUT_STATE_HIDDEN,
    DXUT_STATE_FOCUS,
    DXUT_STATE_MOUSEOVER,
    DXUT_STATE_PRESSED,
};

#define MAX_CONTROL_STATES 6

struct DXUTBlendColor
{
    void        Init( D3DCOLOR defaultColor, D3DCOLOR disabledColor = D3DCOLOR_ARGB( 200, 128, 128, 128 ),
                      D3DCOLOR hiddenColor = 0 );
    void        Blend( UINT iState, float fElapsedTime, float fRate = 0.7f );

    D3DCOLOR    States[ MAX_CONTROL_STATES ]; // Modulate colors for all possible control states
    D3DXCOLOR Current;
};


//-----------------------------------------------------------------------------
// Contains all the display tweakables for a sub-control
//-----------------------------------------------------------------------------
class CDXUTElement
{
public:
    void    SetTexture( UINT iTexture, RECT* prcTexture, D3DCOLOR defaultTextureColor = D3DCOLOR_ARGB( 255, 255, 255,
                                                                                                       255 ) );

    void    Refresh();
    UINT iTexture;          // Index of the texture for this Element 
    RECT rcTexture;         // Bounding rect of this element on the composite textur
    DXUTBlendColor TextureColor;
};


//-----------------------------------------------------------------------------
// All controls must be assigned to a dialog, which handles
// input and rendering for the controls.
//-----------------------------------------------------------------------------
class CDXUTDialog
{
    friend class CDXUTDialogResourceManager;

public:
                        CDXUTDialog();
                        ~CDXUTDialog();

    // Need to call this now
    void                Init( CDXUTDialogResourceManager* pManager, bool bRegisterDialog = true );
    void                Init( CDXUTDialogResourceManager* pManager, bool bRegisterDialog,
                              LPCWSTR pszControlTextureFilename );
    void                Init( CDXUTDialogResourceManager* pManager, bool bRegisterDialog,
                              LPCWSTR szControlTextureResourceName, HMODULE hControlTextureResourceModule );

    // Windows message handler
    bool                MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

    // Control creation
    HRESULT             AddButton( int ID, LPCWSTR strText, int x, int y, int width, int height, UINT nHotkey=0,
                                   bool bIsDefault=false, CDXUTButton** ppCreated=NULL );
 
    HRESULT             AddControl( CDXUTControl* pControl );
    HRESULT             InitControl( CDXUTControl* pControl );

    CDXUTControl* GetControl( int ID );
    CDXUTControl* GetControl( int ID, UINT nControlType );
    CDXUTControl* GetControlAtPoint( POINT pt );

    bool                GetControlEnabled( int ID );
    void                SetControlEnabled( int ID, bool bEnabled );

    // Access the default display Elements used when adding new controls
    HRESULT             SetDefaultElement( UINT nControlType, UINT iElement, CDXUTElement* pElement );
    CDXUTElement* GetDefaultElement( UINT nControlType, UINT iElement );

    // Methods called by controls
    void                SendEvent( UINT nEvent, bool bTriggeredByUser, CDXUTControl* pControl );
    void                RequestFocus( CDXUTControl* pControl );

    // Render helpers
    HRESULT             DrawRect( RECT* pRect, D3DCOLOR color );
    HRESULT             DrawSprite( CDXUTElement* pElement, RECT* prcDest, float fDepth );
    HRESULT             DrawSprite11( CDXUTElement* pElement, RECT* prcDest, float fDepth );

    // Attributes
    bool                GetVisible()
    {
        return m_bVisible;
    }
    void                SetVisible( bool bVisible )
    {
        m_bVisible = bVisible;
    }
    bool                GetMinimized()
    {
        return m_bMinimized;
    }
    void                SetMinimized( bool bMinimized )
    {
        m_bMinimized = bMinimized;
    }
    void                SetBackgroundColors( D3DCOLOR colorAllCorners )
    {
        SetBackgroundColors( colorAllCorners, colorAllCorners, colorAllCorners, colorAllCorners );
    }
    void                SetBackgroundColors( D3DCOLOR colorTopLeft, D3DCOLOR colorTopRight, D3DCOLOR colorBottomLeft,
                                             D3DCOLOR colorBottomRight );
    void                EnableCaption( bool bEnable )
    {
        m_bCaption = bEnable;
    }
    int                 GetCaptionHeight() const
    {
        return m_nCaptionHeight;
    }
    void                SetCaptionHeight( int nHeight )
    {
        m_nCaptionHeight = nHeight;
    }
    void                SetCaptionText( const WCHAR* pwszText )
    {
        wcscpy_s( m_wszCaption, sizeof( m_wszCaption ) / sizeof( m_wszCaption[0] ), pwszText );
    }
    void                GetLocation( POINT& Pt ) const
    {
        Pt.x = m_x; Pt.y = m_y;
    }
    void                SetLocation( int x, int y )
    {
        m_x = x; m_y = y;
    }
    void                SetSize( int width, int height )
    {
        m_width = width; m_height = height;
    }
    int                 GetWidth()
    {
        return m_width;
    }
    int                 GetHeight()
    {
        return m_height;
    }

    static void WINAPI  SetRefreshTime( float fTime )
    {
        s_fTimeRefresh = fTime;
    }

    static CDXUTControl* WINAPI GetNextControl( CDXUTControl* pControl );
    static CDXUTControl* WINAPI GetPrevControl( CDXUTControl* pControl );

    void                RemoveControl( int ID );
    void                RemoveAllControls();

    // Sets the callback used to notify the app of control events
    void                SetCallback( PCALLBACKDXUTGUIEVENT pCallback, void* pUserContext = NULL );
    void                EnableNonUserEvents( bool bEnable )
    {
        m_bNonUserEvents = bEnable;
    }
    void                EnableKeyboardInput( bool bEnable )
    {
        m_bKeyboardInput = bEnable;
    }
    void                EnableMouseInput( bool bEnable )
    {
        m_bMouseInput = bEnable;
    }
    bool                IsKeyboardInputEnabled() const
    {
        return m_bKeyboardInput;
    }

    // Device state notification
    void                Refresh();
    HRESULT             OnRender( float fElapsedTime );


    HRESULT             SetTexture( UINT index, LPCWSTR strFilename );
    HRESULT             SetTexture( UINT index, LPCWSTR strResourceName, HMODULE hResourceModule );
    DXUTTextureNode* GetTexture( UINT index );

    CDXUTDialogResourceManager* GetManager()
    {
        return m_pManager;
    }

    static void WINAPI  ClearFocus();
    void                FocusDefaultControl();

    bool m_bNonUserEvents;
    bool m_bKeyboardInput;
    bool m_bMouseInput;

private:
    int m_nDefaultControlID;

    HRESULT             OnRender10( float fElapsedTime );
    HRESULT             OnRender11( float fElapsedTime );

    static double s_fTimeRefresh;
    double m_fTimeLastRefresh;

    // Initialize default Elements
    void                InitDefaultElements();

    // Windows message handlers
    void                OnMouseMove( POINT pt );
    void                OnMouseUp( POINT pt );

    void                SetNextDialog( CDXUTDialog* pNextDialog );

    // Control events
    bool                OnCycleFocus( bool bForward );

    static CDXUTControl* s_pControlFocus;        // The control which has focus
    static CDXUTControl* s_pControlPressed;      // The control currently pressed

    CDXUTControl* m_pControlMouseOver;           // The control which is hovered over

    bool m_bVisible;
    bool m_bCaption;
    bool m_bMinimized;
    bool m_bDrag;
    WCHAR               m_wszCaption[256];

    int m_x;
    int m_y;
    int m_width;
    int m_height;
    int m_nCaptionHeight;

    D3DCOLOR m_colorTopLeft;
    D3DCOLOR m_colorTopRight;
    D3DCOLOR m_colorBottomLeft;
    D3DCOLOR m_colorBottomRight;

    CDXUTDialogResourceManager* m_pManager;
    PCALLBACKDXUTGUIEVENT m_pCallbackEvent;
    void* m_pCallbackEventUserContext;

    CGrowableArray <int> m_Textures;   // Index into m_TextureCache;
    CGrowableArray <int> m_Fonts;      // Index into m_FontCache;

    CGrowableArray <CDXUTControl*> m_Controls;
    CGrowableArray <DXUTElementHolder*> m_DefaultElements;

    CDXUTElement m_CapElement;  // Element for the caption

    CDXUTDialog* m_pNextDialog;
    CDXUTDialog* m_pPrevDialog;
};


//--------------------------------------------------------------------------------------
// Structs for shared resources
//--------------------------------------------------------------------------------------
struct DXUTTextureNode
{
    bool bFileSource;  // True if this texture is loaded from a file. False if from resource.
    HMODULE hResourceModule;
    int nResourceID;   // Resource ID. If 0, string-based ID is used and stored in strFilename.
    WCHAR strFilename[MAX_PATH];
    DWORD dwWidth;
    DWORD dwHeight;
    IDirect3DTexture9* pTexture9;
    ID3D11Texture2D* pTexture11;
    ID3D11ShaderResourceView* pTexResView11;
};

struct DXUTFontNode
{
    WCHAR strFace[MAX_PATH];
    LONG nHeight;
    LONG nWeight;
    ID3DXFont* pFont9;
};

struct DXUTSpriteVertex
{
    D3DXVECTOR3 vPos;
    D3DXCOLOR vColor;
    D3DXVECTOR2 vTex;
};

//-----------------------------------------------------------------------------
// Manages shared resources of dialogs
//-----------------------------------------------------------------------------
class CDXUTDialogResourceManager
{
public:
            CDXUTDialogResourceManager();
            ~CDXUTDialogResourceManager();

    bool    MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

    // D3D11 specific
    HRESULT OnD3D11CreateDevice( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3d11DeviceContext );
    HRESULT OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc );
    void    OnD3D11ReleasingSwapChain();
    void    OnD3D11DestroyDevice();
    void    StoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext );
    void    RestoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext );
    void    ApplyRenderUI11( ID3D11DeviceContext* pd3dImmediateContext );
    void	ApplyRenderUIUntex11( ID3D11DeviceContext* pd3dImmediateContext );
    void	BeginSprites11( );
    void	EndSprites11( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext );
    ID3D11Device* GetD3D11Device()
    {
        return m_pd3d11Device;
    }
    ID3D11DeviceContext* GetD3D11DeviceContext()
    {
        return m_pd3d11DeviceContext;
    }

    DXUTFontNode* GetFontNode( int iIndex )
    {
        return m_FontCache.GetAt( iIndex );
    };
    DXUTTextureNode* GetTextureNode( int iIndex )
    {
        return m_TextureCache.GetAt( iIndex );
    };

    int     AddFont( LPCWSTR strFaceName, LONG height, LONG weight );
    int     AddTexture( LPCWSTR strFilename );
    int     AddTexture( LPCWSTR strResourceName, HMODULE hResourceModule );

    bool    RegisterDialog( CDXUTDialog* pDialog );
    void    UnregisterDialog( CDXUTDialog* pDialog );
    void    EnableKeyboardInputForAllDialogs();

    // Shared between all dialogs

    // D3D9
    IDirect3DStateBlock9* m_pStateBlock;
    ID3DXSprite* m_pSprite;          // Sprite used for drawing

    // D3D11
    // Shaders
    ID3D11VertexShader* m_pVSRenderUI11;
    ID3D11PixelShader* m_pPSRenderUI11;
    ID3D11PixelShader* m_pPSRenderUIUntex11;

    // States
    ID3D11DepthStencilState* m_pDepthStencilStateUI11;
    ID3D11RasterizerState* m_pRasterizerStateUI11;
    ID3D11BlendState* m_pBlendStateUI11;
    ID3D11SamplerState* m_pSamplerStateUI11;

    // Stored states
    ID3D11DepthStencilState* m_pDepthStencilStateStored11;
    UINT m_StencilRefStored11;
    ID3D11RasterizerState* m_pRasterizerStateStored11;
    ID3D11BlendState* m_pBlendStateStored11;
    float m_BlendFactorStored11[4];
    UINT m_SampleMaskStored11;
    ID3D11SamplerState* m_pSamplerStateStored11;

    ID3D11InputLayout* m_pInputLayout11;
    ID3D11Buffer* m_pVBScreenQuad11;

    // Sprite workaround
    ID3D11Buffer* m_pSpriteBuffer11;
    UINT m_SpriteBufferBytes11;
    CGrowableArray<DXUTSpriteVertex> m_SpriteVertices;

    UINT m_nBackBufferWidth;
    UINT m_nBackBufferHeight;

    CGrowableArray <CDXUTDialog*> m_Dialogs;            // Dialogs registered

protected:
    // D3D11 specific
    ID3D11Device* m_pd3d11Device;
    ID3D11DeviceContext* m_pd3d11DeviceContext;
    HRESULT CreateFont11( UINT index );
    HRESULT CreateTexture11( UINT index );

    CGrowableArray <DXUTTextureNode*> m_TextureCache;   // Shared textures
    CGrowableArray <DXUTFontNode*> m_FontCache;         // Shared fonts
};

//-----------------------------------------------------------------------------
// Base class for controls
//-----------------------------------------------------------------------------
class CDXUTControl
{
public:
                    CDXUTControl( CDXUTDialog* pDialog = NULL );
    virtual         ~CDXUTControl();

    virtual HRESULT OnInit()
    {
        return S_OK;
    }
    virtual void    Refresh();
    virtual void    Render( float fElapsedTime )
    {
    };

    // Windows message handler
    virtual bool    MsgProc( UINT uMsg, WPARAM wParam, LPARAM lParam )
    {
        return false;
    }

    virtual bool    HandleKeyboard( UINT uMsg, WPARAM wParam, LPARAM lParam )
    {
        return false;
    }
    virtual bool    HandleMouse( UINT uMsg, POINT pt, WPARAM wParam, LPARAM lParam )
    {
        return false;
    }

    virtual bool    CanHaveFocus()
    {
        return false;
    }
    virtual void    OnFocusIn()
    {
        m_bHasFocus = true;
    }
    virtual void    OnFocusOut()
    {
        m_bHasFocus = false;
    }
    virtual void    OnMouseEnter()
    {
        m_bMouseOver = true;
    }
    virtual void    OnMouseLeave()
    {
        m_bMouseOver = false;
    }
    virtual void    OnHotkey()
    {
    }

    virtual BOOL    ContainsPoint( POINT pt )
    {
        return PtInRect( &m_rcBoundingBox, pt );
    }

    virtual void    SetEnabled( bool bEnabled )
    {
        m_bEnabled = bEnabled;
    }
    virtual bool    GetEnabled()
    {
        return m_bEnabled;
    }
    virtual void    SetVisible( bool bVisible )
    {
        m_bVisible = bVisible;
    }
    virtual bool    GetVisible()
    {
        return m_bVisible;
    }

    UINT            GetType() const
    {
        return m_Type;
    }

    int             GetID() const
    {
        return m_ID;
    }
    void            SetID( int ID )
    {
        m_ID = ID;
    }

    void            SetLocation( int x, int y )
    {
        m_x = x; m_y = y; UpdateRects();
    }
    void            SetSize( int width, int height )
    {
        m_width = width; m_height = height; UpdateRects();
    }

    void            SetHotkey( UINT nHotkey )
    {
        m_nHotkey = nHotkey;
    }
    UINT            GetHotkey()
    {
        return m_nHotkey;
    }

    void            SetUserData( void* pUserData )
    {
        m_pUserData = pUserData;
    }
    void* GetUserData() const
    {
        return m_pUserData;
    }

    CDXUTElement* GetElement( UINT iElement )
    {
        return m_Elements.GetAt( iElement );
    }
    HRESULT         SetElement( UINT iElement, CDXUTElement* pElement );

    bool m_bVisible;                // Shown/hidden flag
    bool m_bMouseOver;              // Mouse pointer is above control
    bool m_bHasFocus;               // Control has input focus
    bool m_bIsDefault;              // Is the default control

    // Size, scale, and positioning members
    int m_x, m_y;
    int m_width, m_height;

    // These members are set by the container
    CDXUTDialog* m_pDialog;    // Parent container
    UINT m_Index;              // Index within the control list

    CGrowableArray <CDXUTElement*> m_Elements;  // All display elements

protected:
    virtual void    UpdateRects();

    int m_ID;                 // ID number
    DXUT_CONTROL_TYPE m_Type;  // Control type, set once in constructor  
    UINT m_nHotkey;            // Virtual key code for this control's hotkey
    void* m_pUserData;         // Data associated with this control that is set by user.

    bool m_bEnabled;           // Enabled/disabled flag

    RECT m_rcBoundingBox;      // Rectangle defining the active region of the control
};


//-----------------------------------------------------------------------------
// Contains all the display information for a given control type
//-----------------------------------------------------------------------------
struct DXUTElementHolder
{
    UINT nControlType;
    UINT iElement;

    CDXUTElement Element;
};
//-----------------------------------------------------------------------------
// Button control
//-----------------------------------------------------------------------------
class CDXUTButton : public CDXUTControl
{
public:
                    CDXUTButton( CDXUTDialog* pDialog = NULL );

    virtual bool    HandleKeyboard( UINT uMsg, WPARAM wParam, LPARAM lParam );
    virtual bool    HandleMouse( UINT uMsg, POINT pt, WPARAM wParam, LPARAM lParam );

    virtual void    Render( float fElapsedTime );

protected:
    bool m_bPressed;
};