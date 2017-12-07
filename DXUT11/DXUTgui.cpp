//--------------------------------------------------------------------------------------
// File: DXUTgui.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "DXUTres.h"

#include "SDKMisc.h"

#undef min // use __min instead
#undef max // use __max instead

#ifndef WM_XBUTTONDOWN
#define WM_XBUTTONDOWN 0x020B // (not always defined)
#endif
#ifndef WM_XBUTTONUP
#define WM_XBUTTONUP 0x020C // (not always defined)
#endif
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020A // (not always defined)
#endif
#ifndef WHEEL_DELTA
#define WHEEL_DELTA 120 // (not always defined)
#endif

// Minimum scroll bar thumb size
#define SCROLLBAR_MINTHUMBSIZE 8

// Delay and repeat period when clicking on the scroll bar arrows
#define SCROLLBAR_ARROWCLICK_DELAY  0.33
#define SCROLLBAR_ARROWCLICK_REPEAT 0.05

#define DXUT_NEAR_BUTTON_DEPTH 0.6f
#define DXUT_FAR_BUTTON_DEPTH 0.8f

#define DXUT_MAX_GUI_SPRITES 500

D3DCOLORVALUE D3DCOLOR_TO_D3DCOLORVALUE( D3DCOLOR c )
{
    D3DCOLORVALUE cv =
    {
        ( ( c >> 16 ) & 0xFF ) / 255.0f,
        ( ( c >> 8 ) & 0xFF ) / 255.0f,
        ( c & 0xFF ) / 255.0f,
        ( ( c >> 24 ) & 0xFF ) / 255.0f
    };
    return cv;
}

#define UNISCRIBE_DLLNAME L"usp10.dll"

#define GETPROCADDRESS( Module, APIName, Temp ) \
    Temp = GetProcAddress( Module, #APIName ); \
    if( Temp ) \
        *(FARPROC*)&_##APIName = Temp

#define PLACEHOLDERPROC( APIName ) \
    _##APIName = Dummy_##APIName

#define IMM32_DLLNAME L"imm32.dll"
#define VER_DLLNAME L"version.dll"

CHAR g_strUIEffectFile[] = \
    "Texture2D g_Texture;"\
    ""\
    "SamplerState Sampler"\
    "{"\
    "    Filter = MIN_MAG_MIP_LINEAR;"\
    "    AddressU = Wrap;"\
    "    AddressV = Wrap;"\
    "};"\
    ""\
    "BlendState UIBlend"\
    "{"\
    "    AlphaToCoverageEnable = FALSE;"\
    "    BlendEnable[0] = TRUE;"\
    "    SrcBlend = SRC_ALPHA;"\
    "    DestBlend = INV_SRC_ALPHA;"\
    "    BlendOp = ADD;"\
    "    SrcBlendAlpha = ONE;"\
    "    DestBlendAlpha = ZERO;"\
    "    BlendOpAlpha = ADD;"\
    "    RenderTargetWriteMask[0] = 0x0F;"\
    "};"\
    ""\
    "BlendState NoBlending"\
    "{"\
    "    BlendEnable[0] = FALSE;"\
    "    RenderTargetWriteMask[0] = 0x0F;"\
    "};"\
    ""\
    "DepthStencilState DisableDepth"\
    "{"\
    "    DepthEnable = false;"\
    "};"\
    "DepthStencilState EnableDepth"\
    "{"\
    "    DepthEnable = true;"\
    "};"\
    "struct VS_OUTPUT"\
    "{"\
    "    float4 Pos : POSITION;"\
    "    float4 Dif : COLOR;"\
    "    float2 Tex : TEXCOORD;"\
    "};"\
    ""\
    "VS_OUTPUT VS( float3 vPos : POSITION,"\
    "              float4 Dif : COLOR,"\
    "              float2 vTexCoord0 : TEXCOORD )"\
    "{"\
    "    VS_OUTPUT Output;"\
    ""\
    "    Output.Pos = float4( vPos, 1.0f );"\
    "    Output.Dif = Dif;"\
    "    Output.Tex = vTexCoord0;"\
    ""\
    "    return Output;"\
    "}"\
    ""\
    "float4 PS( VS_OUTPUT In ) : SV_Target"\
    "{"\
    "    return g_Texture.Sample( Sampler, In.Tex ) * In.Dif;"\
    "}"\
    ""\
    "float4 PSUntex( VS_OUTPUT In ) : SV_Target"\
    "{"\
    "    return In.Dif;"\
    "}"\
    ""\
    "technique10 RenderUI"\
    "{"\
    "    pass P0"\
    "    {"\
    "        SetVertexShader( CompileShader( vs_4_0, VS() ) );"\
    "        SetGeometryShader( NULL );"\
    "        SetPixelShader( CompileShader( ps_4_0, PS() ) );"\
    "        SetDepthStencilState( DisableDepth, 0 );"\
    "        SetBlendState( UIBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );"\
    "    }"\
    "}"\
    "technique10 RenderUIUntex"\
    "{"\
    "    pass P0"\
    "    {"\
    "        SetVertexShader( CompileShader( vs_4_0, VS() ) );"\
    "        SetGeometryShader( NULL );"\
    "        SetPixelShader( CompileShader( ps_4_0, PSUntex() ) );"\
    "        SetDepthStencilState( DisableDepth, 0 );"\
    "        SetBlendState( UIBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );"\
    "    }"\
    "}"\
    "technique10 RestoreState"\
    "{"\
    "    pass P0"\
    "    {"\
    "        SetDepthStencilState( EnableDepth, 0 );"\
    "        SetBlendState( NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );"\
    "    }"\
    "}";
const UINT              g_uUIEffectFileSize = sizeof( g_strUIEffectFile );


// DXUT_MAX_EDITBOXLENGTH is the maximum string length allowed in edit boxes,
// including the NULL terminator.
// 
// Uniscribe does not support strings having bigger-than-16-bits length.
// This means that the string must be less than 65536 characters long,
// including the NULL terminator.
#define DXUT_MAX_EDITBOXLENGTH 0xFFFF


double                  CDXUTDialog::s_fTimeRefresh = 0.0f;
CDXUTControl*           CDXUTDialog::s_pControlFocus = NULL;        // The control which has focus
CDXUTControl*           CDXUTDialog::s_pControlPressed = NULL;      // The control currently pressed


struct DXUT_SCREEN_VERTEX
{
    float x, y, z, h;
    D3DCOLOR color;
    float tu, tv;

    static DWORD FVF;
};
DWORD                   DXUT_SCREEN_VERTEX::FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;


struct DXUT_SCREEN_VERTEX_UNTEX
{
    float x, y, z, h;
    D3DCOLOR color;

    static DWORD FVF;
};
DWORD                   DXUT_SCREEN_VERTEX_UNTEX::FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;


struct DXUT_SCREEN_VERTEX_10
{
    float x, y, z;
    D3DCOLORVALUE color;
    float tu, tv;
};


inline int RectWidth( RECT& rc )
{
    return ( ( rc ).right - ( rc ).left );
}
inline int RectHeight( RECT& rc )
{
    return ( ( rc ).bottom - ( rc ).top );
}

//--------------------------------------------------------------------------------------
// CDXUTDialog class
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
CDXUTDialog::CDXUTDialog()
{
    m_x = 0;
    m_y = 0;
    m_width = 0;
    m_height = 0;

    m_pManager = NULL;
    m_bVisible = true;
    m_bCaption = false;
    m_bMinimized = false;
    m_bDrag = false;
    m_wszCaption[0] = L'\0';
    m_nCaptionHeight = 18;

    m_colorTopLeft = 0;
    m_colorTopRight = 0;
    m_colorBottomLeft = 0;
    m_colorBottomRight = 0;

    m_pCallbackEvent = NULL;
    m_pCallbackEventUserContext = NULL;

    m_fTimeLastRefresh = 0;

    m_pControlMouseOver = NULL;

    m_pNextDialog = this;
    m_pPrevDialog = this;

    m_nDefaultControlID = 0xffff;
    m_bNonUserEvents = false;
    m_bKeyboardInput = false;
    m_bMouseInput = true;
}


//--------------------------------------------------------------------------------------
CDXUTDialog::~CDXUTDialog()
{
    int i = 0;

    RemoveAllControls();

    m_Fonts.RemoveAll();
    m_Textures.RemoveAll();

    for( i = 0; i < m_DefaultElements.GetSize(); i++ )
    {
        DXUTElementHolder* pElementHolder = m_DefaultElements.GetAt( i );
        SAFE_DELETE( pElementHolder );
    }

    m_DefaultElements.RemoveAll();
}


//--------------------------------------------------------------------------------------
void CDXUTDialog::Init( CDXUTDialogResourceManager* pManager, bool bRegisterDialog )
{
    m_pManager = pManager;
    if( bRegisterDialog )
        pManager->RegisterDialog( this );

    SetTexture( 0, MAKEINTRESOURCE( 0xFFFF ), ( HMODULE )0xFFFF );
    InitDefaultElements();
}


//--------------------------------------------------------------------------------------
void CDXUTDialog::Init( CDXUTDialogResourceManager* pManager, bool bRegisterDialog, LPCWSTR pszControlTextureFilename )
{
    m_pManager = pManager;
    if( bRegisterDialog )
        pManager->RegisterDialog( this );
    SetTexture( 0, pszControlTextureFilename );
    InitDefaultElements();
}


//--------------------------------------------------------------------------------------
void CDXUTDialog::Init( CDXUTDialogResourceManager* pManager, bool bRegisterDialog,
                        LPCWSTR szControlTextureResourceName, HMODULE hControlTextureResourceModule )
{
    m_pManager = pManager;
    if( bRegisterDialog )
        pManager->RegisterDialog( this );

    SetTexture( 0, szControlTextureResourceName, hControlTextureResourceModule );
    InitDefaultElements();
}


//--------------------------------------------------------------------------------------
void CDXUTDialog::SetCallback( PCALLBACKDXUTGUIEVENT pCallback, void* pUserContext )
{
    // If this assert triggers, you need to call CDXUTDialog::Init() first.  This change
    // was made so that the DXUT's GUI could become seperate and optional from DXUT's core.  The 
    // creation and interfacing with CDXUTDialogResourceManager is now the responsibility 
    // of the application if it wishes to use DXUT's GUI.
    assert( m_pManager != NULL && L"To fix call CDXUTDialog::Init() first.  See comments for details." );

    m_pCallbackEvent = pCallback;
    m_pCallbackEventUserContext = pUserContext;
}


//--------------------------------------------------------------------------------------
void CDXUTDialog::RemoveControl( int ID )
{
    for( int i = 0; i < m_Controls.GetSize(); i++ )
    {
        CDXUTControl* pControl = m_Controls.GetAt( i );
        if( pControl->GetID() == ID )
        {
            // Clean focus first
            ClearFocus();

            // Clear references to this control
            if( s_pControlFocus == pControl )
                s_pControlFocus = NULL;
            if( s_pControlPressed == pControl )
                s_pControlPressed = NULL;
            if( m_pControlMouseOver == pControl )
                m_pControlMouseOver = NULL;

            SAFE_DELETE( pControl );
            m_Controls.Remove( i );

            return;
        }
    }
}


//--------------------------------------------------------------------------------------
void CDXUTDialog::RemoveAllControls()
{
    if( s_pControlFocus && s_pControlFocus->m_pDialog == this )
        s_pControlFocus = NULL;
    if( s_pControlPressed && s_pControlPressed->m_pDialog == this )
        s_pControlPressed = NULL;
    m_pControlMouseOver = NULL;

    for( int i = 0; i < m_Controls.GetSize(); i++ )
    {
        CDXUTControl* pControl = m_Controls.GetAt( i );
        SAFE_DELETE( pControl );
    }

    m_Controls.RemoveAll();
}


//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager::CDXUTDialogResourceManager()
{
    // Begin D3D11-specific
    // Shaders
    m_pVSRenderUI11 = NULL;
    m_pPSRenderUI11 = NULL;
    m_pPSRenderUIUntex11 = NULL;

    // States
    m_pDepthStencilStateUI11 = NULL;
    m_pRasterizerStateUI11 = NULL;
    m_pBlendStateUI11 = NULL;
    m_pSamplerStateUI11 = NULL;
    m_pDepthStencilStateStored11 = NULL;
    m_pRasterizerStateStored11 = NULL;
    m_pBlendStateStored11 = NULL;
    m_pSamplerStateStored11 = NULL;

    m_pInputLayout11 = NULL;
    m_pVBScreenQuad11 = NULL;
    m_pSpriteBuffer11 = NULL;
}


//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager::~CDXUTDialogResourceManager()
{
    int i;
    for( i = 0; i < m_FontCache.GetSize(); i++ )
    {
        DXUTFontNode* pFontNode = m_FontCache.GetAt( i );
        SAFE_DELETE( pFontNode );
    }
    m_FontCache.RemoveAll();

    for( i = 0; i < m_TextureCache.GetSize(); i++ )
    {
        DXUTTextureNode* pTextureNode = m_TextureCache.GetAt( i );
        SAFE_DELETE( pTextureNode );
    }
    m_TextureCache.RemoveAll();
}

//--------------------------------------------------------------------------------------
bool CDXUTDialogResourceManager::MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    return false;
}

HRESULT CDXUTDialogResourceManager::OnD3D11CreateDevice( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3d11DeviceContext )
{
    m_pd3d11Device = pd3dDevice;
    m_pd3d11DeviceContext = pd3d11DeviceContext;

    HRESULT hr = S_OK;

    // Compile Shaders
    ID3DBlob* pVSBlob = NULL;
    ID3DBlob* pPSBlob = NULL;
    ID3DBlob* pPSUntexBlob = NULL;
    V_RETURN( D3DCompile( g_strUIEffectFile, g_uUIEffectFileSize, "none", NULL, NULL, "VS", "vs_4_0_level_9_1", 
         D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY, 0, &pVSBlob, NULL ) );
    V_RETURN( D3DCompile( g_strUIEffectFile, g_uUIEffectFileSize, "none", NULL, NULL, "PS", "ps_4_0_level_9_1", 
         D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY, 0, &pPSBlob, NULL ) );
    V_RETURN( D3DCompile( g_strUIEffectFile, g_uUIEffectFileSize, "none", NULL, NULL, "PSUntex", "ps_4_0_level_9_1", 
         D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY, 0, &pPSUntexBlob, NULL ) );

    // Create Shaders
    V_RETURN( pd3dDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &m_pVSRenderUI11 ) );

    V_RETURN( pd3dDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pPSRenderUI11 ) );

    V_RETURN( pd3dDevice->CreatePixelShader( pPSUntexBlob->GetBufferPointer(), pPSUntexBlob->GetBufferSize(), NULL, &m_pPSRenderUIUntex11 ) );
 
    // States
    D3D11_DEPTH_STENCIL_DESC DSDesc;
    ZeroMemory( &DSDesc, sizeof( D3D11_DEPTH_STENCIL_DESC ) );
    DSDesc.DepthEnable = FALSE;
    DSDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    DSDesc.DepthFunc = D3D11_COMPARISON_LESS;
    DSDesc.StencilEnable = FALSE;
    V_RETURN( pd3dDevice->CreateDepthStencilState( &DSDesc, &m_pDepthStencilStateUI11 ) );
 
    D3D11_RASTERIZER_DESC RSDesc;
    RSDesc.AntialiasedLineEnable = FALSE;
    RSDesc.CullMode = D3D11_CULL_BACK;
    RSDesc.DepthBias = 0;
    RSDesc.DepthBiasClamp = 0.0f;
    RSDesc.DepthClipEnable = TRUE;
    RSDesc.FillMode = D3D11_FILL_SOLID;
    RSDesc.FrontCounterClockwise = FALSE;
    RSDesc.MultisampleEnable = TRUE;
    RSDesc.ScissorEnable = FALSE;
    RSDesc.SlopeScaledDepthBias = 0.0f;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RSDesc, &m_pRasterizerStateUI11 ) );

    D3D11_BLEND_DESC BSDesc;
    ZeroMemory( &BSDesc, sizeof( D3D11_BLEND_DESC ) );
    
    BSDesc.RenderTarget[0].BlendEnable = TRUE;
    BSDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BSDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BSDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BSDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BSDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    BSDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BSDesc.RenderTarget[0].RenderTargetWriteMask = 0x0F;

    V_RETURN( pd3dDevice->CreateBlendState( &BSDesc, &m_pBlendStateUI11 ) );

    D3D11_SAMPLER_DESC SSDesc;
    ZeroMemory( &SSDesc, sizeof( D3D11_SAMPLER_DESC ) );
    SSDesc.Filter = D3D11_FILTER_ANISOTROPIC   ;
    SSDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SSDesc.MaxAnisotropy = 16;
    SSDesc.MinLOD = 0;
    SSDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if ( pd3dDevice->GetFeatureLevel() < D3D_FEATURE_LEVEL_9_3 ) {
        SSDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        SSDesc.MaxAnisotropy = 0;
    }
    V_RETURN( pd3dDevice->CreateSamplerState( &SSDesc, &m_pSamplerStateUI11 ) );

    // Create the font and texture objects in the cache arrays.
    int i = 0;
    for( i = 0; i < m_FontCache.GetSize(); i++ )
    {
        hr = CreateFont11( i );
        if( FAILED( hr ) )
            return hr;
    }

    for( i = 0; i < m_TextureCache.GetSize(); i++ )
    {
        hr = CreateTexture11( i );
        if( FAILED( hr ) )
            return hr;
    }

    // Create input layout
    const D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",     0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

    V_RETURN( pd3dDevice->CreateInputLayout( layout, ARRAYSIZE( layout ), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pInputLayout11 ) );

    // Release the blobs
    SAFE_RELEASE( pVSBlob );
    SAFE_RELEASE( pPSBlob );
    SAFE_RELEASE( pPSUntexBlob );

    // Create a vertex buffer quad for rendering later
    D3D11_BUFFER_DESC BufDesc;
    BufDesc.ByteWidth = sizeof( DXUT_SCREEN_VERTEX_10 ) * 4;
    BufDesc.Usage = D3D11_USAGE_DYNAMIC;
    BufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    BufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    BufDesc.MiscFlags = 0;
    V_RETURN( pd3dDevice->CreateBuffer( &BufDesc, NULL, &m_pVBScreenQuad11 ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CDXUTDialogResourceManager::OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice,
                                                             const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
{
    HRESULT hr = S_OK;

    m_nBackBufferWidth = pBackBufferSurfaceDesc->Width;
    m_nBackBufferHeight = pBackBufferSurfaceDesc->Height;

    return hr;
}


//--------------------------------------------------------------------------------------
void CDXUTDialogResourceManager::OnD3D11ReleasingSwapChain()
{
}


//--------------------------------------------------------------------------------------
void CDXUTDialogResourceManager::OnD3D11DestroyDevice()
{
    int i;

    // Release the resources but don't clear the cache, as these will need to be
    // recreated if the device is recreated

    for( i = 0; i < m_TextureCache.GetSize(); i++ )
    {
        DXUTTextureNode* pTextureNode = m_TextureCache.GetAt( i );
        SAFE_RELEASE( pTextureNode->pTexResView11 );
        SAFE_RELEASE( pTextureNode->pTexture11 );
    }

    // D3D11
    SAFE_RELEASE( m_pVBScreenQuad11 );
    SAFE_RELEASE( m_pSpriteBuffer11 );
    m_SpriteBufferBytes11 = 0;
    SAFE_RELEASE( m_pInputLayout11 );

    // Shaders
    SAFE_RELEASE( m_pVSRenderUI11 );
    SAFE_RELEASE( m_pPSRenderUI11 );
    SAFE_RELEASE( m_pPSRenderUIUntex11 );

    // States
    SAFE_RELEASE( m_pDepthStencilStateUI11 );
    SAFE_RELEASE( m_pRasterizerStateUI11 );
    SAFE_RELEASE( m_pBlendStateUI11 );
    SAFE_RELEASE( m_pSamplerStateUI11 );

    SAFE_RELEASE( m_pDepthStencilStateStored11 );
    SAFE_RELEASE( m_pRasterizerStateStored11 );
    SAFE_RELEASE( m_pBlendStateStored11 );
    SAFE_RELEASE( m_pSamplerStateStored11 );
}

//--------------------------------------------------------------------------------------
void CDXUTDialogResourceManager::StoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext )
{
    pd3dImmediateContext->OMGetDepthStencilState( &m_pDepthStencilStateStored11, &m_StencilRefStored11 );
    pd3dImmediateContext->RSGetState( &m_pRasterizerStateStored11 );
    pd3dImmediateContext->OMGetBlendState( &m_pBlendStateStored11, m_BlendFactorStored11, &m_SampleMaskStored11 );
    pd3dImmediateContext->PSGetSamplers( 0, 1, &m_pSamplerStateStored11 );
}

//--------------------------------------------------------------------------------------
void CDXUTDialogResourceManager::RestoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext )
{
    pd3dImmediateContext->OMSetDepthStencilState( m_pDepthStencilStateStored11, m_StencilRefStored11 );
    pd3dImmediateContext->RSSetState( m_pRasterizerStateStored11 );
    pd3dImmediateContext->OMSetBlendState( m_pBlendStateStored11, m_BlendFactorStored11, m_SampleMaskStored11 );
    pd3dImmediateContext->PSSetSamplers( 0, 1, &m_pSamplerStateStored11 );

    SAFE_RELEASE( m_pDepthStencilStateStored11 );
    SAFE_RELEASE( m_pRasterizerStateStored11 );
    SAFE_RELEASE( m_pBlendStateStored11 );
    SAFE_RELEASE( m_pSamplerStateStored11 );
}

//--------------------------------------------------------------------------------------
void CDXUTDialogResourceManager::ApplyRenderUI11( ID3D11DeviceContext* pd3dImmediateContext )
{
    // Shaders
    pd3dImmediateContext->VSSetShader( m_pVSRenderUI11, NULL, 0 );
    pd3dImmediateContext->HSSetShader( NULL, NULL, 0 );
    pd3dImmediateContext->DSSetShader( NULL, NULL, 0 );
    pd3dImmediateContext->GSSetShader( NULL, NULL, 0 );
    pd3dImmediateContext->PSSetShader( m_pPSRenderUI11, NULL, 0 );

    // States
    pd3dImmediateContext->OMSetDepthStencilState( m_pDepthStencilStateUI11, 0 );
    pd3dImmediateContext->RSSetState( m_pRasterizerStateUI11 );
    float BlendFactor[4] = { 0, 0, 0, 0 };
    pd3dImmediateContext->OMSetBlendState( m_pBlendStateUI11, BlendFactor, 0xFFFFFFFF );
    pd3dImmediateContext->PSSetSamplers( 0, 1, &m_pSamplerStateUI11 );
}

//--------------------------------------------------------------------------------------
void CDXUTDialogResourceManager::ApplyRenderUIUntex11( ID3D11DeviceContext* pd3dImmediateContext )
{
    // Shaders
    pd3dImmediateContext->VSSetShader( m_pVSRenderUI11, NULL, 0 );
    pd3dImmediateContext->HSSetShader( NULL, NULL, 0 );
    pd3dImmediateContext->DSSetShader( NULL, NULL, 0 );
    pd3dImmediateContext->GSSetShader( NULL, NULL, 0 );
    pd3dImmediateContext->PSSetShader( m_pPSRenderUIUntex11, NULL, 0 );

    // States
    pd3dImmediateContext->OMSetDepthStencilState( m_pDepthStencilStateUI11, 0 );
    pd3dImmediateContext->RSSetState( m_pRasterizerStateUI11 );
    float BlendFactor[4] = { 0, 0, 0, 0 };
    pd3dImmediateContext->OMSetBlendState( m_pBlendStateUI11, BlendFactor, 0xFFFFFFFF );
    pd3dImmediateContext->PSSetSamplers( 0, 1, &m_pSamplerStateUI11 );
}

//--------------------------------------------------------------------------------------
void CDXUTDialogResourceManager::BeginSprites11( )
{
    m_SpriteVertices.Reset();
}

//--------------------------------------------------------------------------------------
void CDXUTDialogResourceManager::EndSprites11( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext )
{

    // ensure our buffer size can hold our sprites
    UINT SpriteDataBytes = m_SpriteVertices.GetSize() * sizeof( DXUTSpriteVertex );
    if( m_SpriteBufferBytes11 < SpriteDataBytes )
    {
        SAFE_RELEASE( m_pSpriteBuffer11 );
        m_SpriteBufferBytes11 = SpriteDataBytes;

        D3D11_BUFFER_DESC BufferDesc;
        BufferDesc.ByteWidth = m_SpriteBufferBytes11;
        BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        BufferDesc.MiscFlags = 0;

        pd3dDevice->CreateBuffer( &BufferDesc, NULL, &m_pSpriteBuffer11 );
    }

    // Copy the sprites over
    D3D11_BOX destRegion;
    destRegion.left = 0;
    destRegion.right = SpriteDataBytes;
    destRegion.top = 0;
    destRegion.bottom = 1;
    destRegion.front = 0;
    destRegion.back = 1;
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    if ( S_OK == pd3dImmediateContext->Map( m_pSpriteBuffer11, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) ) { 
        CopyMemory( MappedResource.pData, (void*)m_SpriteVertices.GetData(), SpriteDataBytes );
        pd3dImmediateContext->Unmap(m_pSpriteBuffer11, 0);
    }

    // Draw
    UINT Stride = sizeof( DXUTSpriteVertex );
    UINT Offset = 0;
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, &m_pSpriteBuffer11, &Stride, &Offset );
    pd3dImmediateContext->IASetInputLayout( m_pInputLayout11 );
    pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    pd3dImmediateContext->Draw( m_SpriteVertices.GetSize(), 0 );

    m_SpriteVertices.Reset();
}

//--------------------------------------------------------------------------------------
bool CDXUTDialogResourceManager::RegisterDialog( CDXUTDialog* pDialog )
{
    // Check that the dialog isn't already registered.
    for( int i = 0; i < m_Dialogs.GetSize(); ++i )
        if( m_Dialogs.GetAt( i ) == pDialog )
            return true;

    // Add to the list.
    if( FAILED( m_Dialogs.Add( pDialog ) ) )
        return false;

    // Set up next and prev pointers.
    if( m_Dialogs.GetSize() > 1 )
        m_Dialogs[m_Dialogs.GetSize() - 2]->SetNextDialog( pDialog );
    m_Dialogs[m_Dialogs.GetSize() - 1]->SetNextDialog( m_Dialogs[0] );

    return true;
}


//--------------------------------------------------------------------------------------
void CDXUTDialogResourceManager::UnregisterDialog( CDXUTDialog* pDialog )
{
    // Search for the dialog in the list.
    for( int i = 0; i < m_Dialogs.GetSize(); ++i )
        if( m_Dialogs.GetAt( i ) == pDialog )
        {
            m_Dialogs.Remove( i );
            if( m_Dialogs.GetSize() > 0 )
            {
                int l, r;

                if( 0 == i )
                    l = m_Dialogs.GetSize() - 1;
                else
                    l = i - 1;

                if( m_Dialogs.GetSize() == i )
                    r = 0;
                else
                    r = i;

                m_Dialogs[l]->SetNextDialog( m_Dialogs[r] );
            }
            return;
        }
}


//--------------------------------------------------------------------------------------
void CDXUTDialogResourceManager::EnableKeyboardInputForAllDialogs()
{
    // Enable keyboard input for all registered dialogs
    for( int i = 0; i < m_Dialogs.GetSize(); ++i )
        m_Dialogs[i]->EnableKeyboardInput( true );
}


//--------------------------------------------------------------------------------------
void CDXUTDialog::Refresh()
{
    if( s_pControlFocus )
        s_pControlFocus->OnFocusOut();

    if( m_pControlMouseOver )
        m_pControlMouseOver->OnMouseLeave();

    s_pControlFocus = NULL;
    s_pControlPressed = NULL;
    m_pControlMouseOver = NULL;

    for( int i = 0; i < m_Controls.GetSize(); i++ )
    {
        CDXUTControl* pControl = m_Controls.GetAt( i );
        pControl->Refresh();
    }

    if( m_bKeyboardInput )
        FocusDefaultControl();
}


//--------------------------------------------------------------------------------------
HRESULT CDXUTDialog::OnRender( float fElapsedTime )
{
    return OnRender11( fElapsedTime );
}

//--------------------------------------------------------------------------------------
HRESULT CDXUTDialog::OnRender11( float fElapsedTime )
{
    // If this assert triggers, you need to call CDXUTDialogResourceManager::On*Device() from inside
    // the application's device callbacks.  See the SDK samples for an example of how to do this.
    assert( m_pManager->GetD3D11Device() &&
            L"To fix hook up CDXUTDialogResourceManager to device callbacks.  See comments for details" );

    // See if the dialog needs to be refreshed
    if( m_fTimeLastRefresh < s_fTimeRefresh )
    {
        m_fTimeLastRefresh = DXUTGetTime();
        Refresh();
    }

    // For invisible dialog, out now.
    if( !m_bVisible ||
        ( m_bMinimized && !m_bCaption ) )
        return S_OK;

    ID3D11Device* pd3dDevice = m_pManager->GetD3D11Device();
    ID3D11DeviceContext* pd3dDeviceContext = m_pManager->GetD3D11DeviceContext();

    // Set up a state block here and restore it when finished drawing all the controls
    m_pManager->StoreD3D11State( pd3dDeviceContext );

    BOOL bBackgroundIsVisible = ( m_colorTopLeft | m_colorTopRight | m_colorBottomRight | m_colorBottomLeft ) &
        0xff000000;
    if( !m_bMinimized && bBackgroundIsVisible )
    {
        // Convert the draw rectangle from screen coordinates to clip space coordinates.
        float Left, Right, Top, Bottom;
        Left = m_x * 2.0f / m_pManager->m_nBackBufferWidth - 1.0f;
        Right = ( m_x + m_width ) * 2.0f / m_pManager->m_nBackBufferWidth - 1.0f;
        Top = 1.0f - m_y * 2.0f / m_pManager->m_nBackBufferHeight;
        Bottom = 1.0f - ( m_y + m_height ) * 2.0f / m_pManager->m_nBackBufferHeight;

        DXUT_SCREEN_VERTEX_10 vertices[4] =
        {
            Left,  Top,    0.5f, D3DCOLOR_TO_D3DCOLORVALUE( m_colorTopLeft ), 0.0f, 0.0f,
            Right, Top,    0.5f, D3DCOLOR_TO_D3DCOLORVALUE( m_colorTopRight ), 1.0f, 0.0f,
            Left,  Bottom, 0.5f, D3DCOLOR_TO_D3DCOLORVALUE( m_colorBottomLeft ), 0.0f, 1.0f,
            Right, Bottom, 0.5f, D3DCOLOR_TO_D3DCOLORVALUE( m_colorBottomRight ), 1.0f, 1.0f,
        };

        //DXUT_SCREEN_VERTEX_10 *pVB;
        D3D11_MAPPED_SUBRESOURCE MappedData;
        if( SUCCEEDED( pd3dDeviceContext->Map( m_pManager->m_pVBScreenQuad11, 0, D3D11_MAP_WRITE_DISCARD,
                                               0, &MappedData ) ) )
        {
            CopyMemory( MappedData.pData, vertices, sizeof( vertices ) );
            pd3dDeviceContext->Unmap( m_pManager->m_pVBScreenQuad11, 0 );
        }

        // Set the quad VB as current
        UINT stride = sizeof( DXUT_SCREEN_VERTEX_10 );
        UINT offset = 0;
        pd3dDeviceContext->IASetVertexBuffers( 0, 1, &m_pManager->m_pVBScreenQuad11, &stride, &offset );
        pd3dDeviceContext->IASetInputLayout( m_pManager->m_pInputLayout11 );
        pd3dDeviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

        // Setup for rendering
        m_pManager->ApplyRenderUIUntex11( pd3dDeviceContext );
        pd3dDeviceContext->Draw( 4, 0 );
    }

    DXUTTextureNode* pTextureNode = GetTexture( 0 );
    pd3dDeviceContext->PSSetShaderResources( 0, 1, &pTextureNode->pTexResView11 );

    // Sort depth back to front
    m_pManager->BeginSprites11();

    m_pManager->ApplyRenderUI11( pd3dDeviceContext );

    // Render the caption if it's enabled.
    if( m_bCaption )
    {
        // DrawSprite will offset the rect down by
        // m_nCaptionHeight, so adjust the rect higher
        // here to negate the effect.
        RECT rc = { 0, -m_nCaptionHeight, m_width, 0 };
        DrawSprite11( &m_CapElement, &rc, 0.99f );
        rc.left += 5; // Make a left margin
        WCHAR wszOutput[256];
        wcscpy_s( wszOutput, 256, m_wszCaption );
        if( m_bMinimized )
            wcscat_s( wszOutput, 256, L" (Minimized)" );
    }

    // If the dialog is minimized, skip rendering
    // its controls.
    if( !m_bMinimized )
    {
        for( int i = 0; i < m_Controls.GetSize(); i++ )
        {
            CDXUTControl* pControl = m_Controls.GetAt( i );

            // Focused control is drawn last
            if( pControl == s_pControlFocus )
                continue;

            pControl->Render( fElapsedTime );
        }

        if( s_pControlFocus != NULL && s_pControlFocus->m_pDialog == this )
            s_pControlFocus->Render( fElapsedTime );
    }

    // End sprites
    if( m_bCaption )
    {
        m_pManager->EndSprites11( pd3dDevice, pd3dDeviceContext );
    }
    m_pManager->RestoreD3D11State( pd3dDeviceContext );

    return S_OK;
}

//--------------------------------------------------------------------------------------
VOID CDXUTDialog::SendEvent( UINT nEvent, bool bTriggeredByUser, CDXUTControl* pControl )
{
    // If no callback has been registered there's nowhere to send the event to
    if( m_pCallbackEvent == NULL )
        return;

    // Discard events triggered programatically if these types of events haven't been
    // enabled
    if( !bTriggeredByUser && !m_bNonUserEvents )
        return;

    m_pCallbackEvent( nEvent, pControl->GetID(), pControl, m_pCallbackEventUserContext );
}


//--------------------------------------------------------------------------------------
int CDXUTDialogResourceManager::AddFont( LPCWSTR strFaceName, LONG height, LONG weight ) 
{
    // See if this font already exists
    for( int i = 0; i < m_FontCache.GetSize(); i++ )
    {
        DXUTFontNode* pFontNode = m_FontCache.GetAt( i );
        size_t nLen = 0;
        nLen = wcsnlen( strFaceName, MAX_PATH);
        if( 0 == _wcsnicmp( pFontNode->strFace, strFaceName, nLen ) &&
            pFontNode->nHeight == height &&
            pFontNode->nWeight == weight )
        {
            return i;
        }
    }

    // Add a new font and try to create it
    DXUTFontNode* pNewFontNode = new DXUTFontNode;
    if( pNewFontNode == NULL )
        return -1;

    ZeroMemory( pNewFontNode, sizeof( DXUTFontNode ) );
    wcscpy_s( pNewFontNode->strFace, MAX_PATH, strFaceName );
    pNewFontNode->nHeight = height;
    pNewFontNode->nWeight = weight;
    m_FontCache.Add( pNewFontNode );

    int iFont = m_FontCache.GetSize() - 1;

    return iFont;
}


//--------------------------------------------------------------------------------------
int CDXUTDialogResourceManager::AddTexture( LPCWSTR strFilename )
{
    // See if this texture already exists
    for( int i = 0; i < m_TextureCache.GetSize(); i++ )
    {
        DXUTTextureNode* pTextureNode = m_TextureCache.GetAt( i );
        size_t nLen = 0;
        nLen = wcsnlen( strFilename, MAX_PATH);
        if( pTextureNode->bFileSource &&  // Sources must match
            0 == _wcsnicmp( pTextureNode->strFilename, strFilename, nLen ) )
        {
            return i;
        }
    }

    // Add a new texture and try to create it
    DXUTTextureNode* pNewTextureNode = new DXUTTextureNode;
    if( pNewTextureNode == NULL )
        return -1;

    ZeroMemory( pNewTextureNode, sizeof( DXUTTextureNode ) );
    pNewTextureNode->bFileSource = true;
    wcscpy_s( pNewTextureNode->strFilename, MAX_PATH, strFilename );

    m_TextureCache.Add( pNewTextureNode );

    int iTexture = m_TextureCache.GetSize() - 1;


    return iTexture;
}


//--------------------------------------------------------------------------------------
int CDXUTDialogResourceManager::AddTexture( LPCWSTR strResourceName, HMODULE hResourceModule )
{
    // See if this texture already exists
    for( int i = 0; i < m_TextureCache.GetSize(); i++ )
    {
        DXUTTextureNode* pTextureNode = m_TextureCache.GetAt( i );
        if( !pTextureNode->bFileSource &&      // Sources must match
            pTextureNode->hResourceModule == hResourceModule ) // Module handles must match
        {
            if( IS_INTRESOURCE( strResourceName ) )
            {
                // Integer-based ID
                if( ( INT_PTR )strResourceName == pTextureNode->nResourceID )
                    return i;
            }
            else
            {
                // String-based ID
                size_t nLen = 0;
                nLen = wcsnlen ( strResourceName, MAX_PATH );
                if( 0 == _wcsnicmp( pTextureNode->strFilename, strResourceName, nLen ) )
                    return i;
            }
        }
    }

    // Add a new texture and try to create it
    DXUTTextureNode* pNewTextureNode = new DXUTTextureNode;
    if( pNewTextureNode == NULL )
        return -1;

    ZeroMemory( pNewTextureNode, sizeof( DXUTTextureNode ) );
    pNewTextureNode->hResourceModule = hResourceModule;
    if( IS_INTRESOURCE( strResourceName ) )
    {
        pNewTextureNode->nResourceID = ( int )( size_t )strResourceName;
    }
    else
    {
        pNewTextureNode->nResourceID = 0;
        wcscpy_s( pNewTextureNode->strFilename, MAX_PATH, strResourceName );
    }

    m_TextureCache.Add( pNewTextureNode );

    int iTexture = m_TextureCache.GetSize() - 1;
    return iTexture;
}


//--------------------------------------------------------------------------------------
HRESULT CDXUTDialog::SetTexture( UINT index, LPCWSTR strFilename )
{
    // If this assert triggers, you need to call CDXUTDialog::Init() first.  This change
    // was made so that the DXUT's GUI could become seperate and optional from DXUT's core.  The 
    // creation and interfacing with CDXUTDialogResourceManager is now the responsibility 
    // of the application if it wishes to use DXUT's GUI.
    assert( m_pManager != NULL && L"To fix this, call CDXUTDialog::Init() first.  See comments for details." );

    // Make sure the list is at least as large as the index being set
    for( UINT i = m_Textures.GetSize(); i <= index; i++ )
    {
        m_Textures.Add( -1 );
    }

    int iTexture = m_pManager->AddTexture( strFilename );

    m_Textures.SetAt( index, iTexture );
    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CDXUTDialog::SetTexture( UINT index, LPCWSTR strResourceName, HMODULE hResourceModule )
{
    // If this assert triggers, you need to call CDXUTDialog::Init() first.  This change
    // was made so that the DXUT's GUI could become seperate and optional from DXUT's core.  The 
    // creation and interfacing with CDXUTDialogResourceManager is now the responsibility 
    // of the application if it wishes to use DXUT's GUI.
    assert( m_pManager != NULL && L"To fix this, call CDXUTDialog::Init() first.  See comments for details." );

    // Make sure the list is at least as large as the index being set
    for( UINT i = m_Textures.GetSize(); i <= index; i++ )
    {
        m_Textures.Add( -1 );
    }

    int iTexture = m_pManager->AddTexture( strResourceName, hResourceModule );

    m_Textures.SetAt( index, iTexture );
    return S_OK;
}


//--------------------------------------------------------------------------------------
DXUTTextureNode* CDXUTDialog::GetTexture( UINT index )
{
    if( NULL == m_pManager )
        return NULL;
    return m_pManager->GetTextureNode( m_Textures.GetAt( index ) );
}



//--------------------------------------------------------------------------------------
bool CDXUTDialog::MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    bool bHandled = false;

    // For invisible dialog, do not handle anything.
    if( !m_bVisible )
        return false;

    // If automation command-line switch is on, enable this dialog's keyboard input
    // upon any key press or mouse click.
    if( DXUTGetAutomation() &&
        ( WM_LBUTTONDOWN == uMsg || WM_LBUTTONDBLCLK == uMsg || WM_KEYDOWN == uMsg ) )
    {
        m_pManager->EnableKeyboardInputForAllDialogs();
    }

    // If caption is enable, check for clicks in the caption area.
    if( m_bCaption )
    {
        if( uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK )
        {
            POINT mousePoint =
            {
                short( LOWORD( lParam ) ), short( HIWORD( lParam ) )
            };

            if( mousePoint.x >= m_x && mousePoint.x < m_x + m_width &&
                mousePoint.y >= m_y && mousePoint.y < m_y + m_nCaptionHeight )
            {
                m_bDrag = true;
                SetCapture( DXUTGetHWND() );
                return true;
            }
        }
        else if( uMsg == WM_LBUTTONUP && m_bDrag )
        {
            POINT mousePoint =
            {
                short( LOWORD( lParam ) ), short( HIWORD( lParam ) )
            };

            if( mousePoint.x >= m_x && mousePoint.x < m_x + m_width &&
                mousePoint.y >= m_y && mousePoint.y < m_y + m_nCaptionHeight )
            {
                ReleaseCapture();
                m_bDrag = false;
                m_bMinimized = !m_bMinimized;
                return true;
            }
        }
    }

    // If the dialog is minimized, don't send any messages to controls.
    if( m_bMinimized )
        return false;

    // If a control is in focus, it belongs to this dialog, and it's enabled, then give
    // it the first chance at handling the message.
    if( s_pControlFocus &&
        s_pControlFocus->m_pDialog == this &&
        s_pControlFocus->GetEnabled() )
    {
        // If the control MsgProc handles it, then we don't.
        if( s_pControlFocus->MsgProc( uMsg, wParam, lParam ) )
            return true;
    }

    switch( uMsg )
    {
        case WM_SIZE:
        case WM_MOVE:
            {
                // Handle sizing and moving messages so that in case the mouse cursor is moved out
                // of an UI control because of the window adjustment, we can properly
                // unhighlight the highlighted control.
                POINT pt =
                {
                    -1, -1
                };
                OnMouseMove( pt );
                break;
            }

        case WM_ACTIVATEAPP:
            // Call OnFocusIn()/OnFocusOut() of the control that currently has the focus
            // as the application is activated/deactivated.  This matches the Windows
            // behavior.
            if( s_pControlFocus &&
                s_pControlFocus->m_pDialog == this &&
                s_pControlFocus->GetEnabled() )
            {
                if( wParam )
                    s_pControlFocus->OnFocusIn();
                else
                    s_pControlFocus->OnFocusOut();
            }
            break;

            // Keyboard messages
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            {
                // If a control is in focus, it belongs to this dialog, and it's enabled, then give
                // it the first chance at handling the message.
                if( s_pControlFocus &&
                    s_pControlFocus->m_pDialog == this &&
                    s_pControlFocus->GetEnabled() )
                {
                    if( s_pControlFocus->HandleKeyboard( uMsg, wParam, lParam ) )
                        return true;
                }

                // Not yet handled, see if this matches a control's hotkey
                // Activate the hotkey if the focus doesn't belong to an
                // edit box.
                if( uMsg == WM_KEYDOWN && ( !s_pControlFocus ||
                                            ( s_pControlFocus->GetType() != DXUT_CONTROL_EDITBOX
                                              && s_pControlFocus->GetType() != DXUT_CONTROL_IMEEDITBOX ) ) )
                {
                    for( int i = 0; i < m_Controls.GetSize(); i++ )
                    {
                        CDXUTControl* pControl = m_Controls.GetAt( i );
                        if( pControl->GetHotkey() == wParam )
                        {
                            pControl->OnHotkey();
                            return true;
                        }
                    }
                }

                // Not yet handled, check for focus messages
                if( uMsg == WM_KEYDOWN )
                {
                    // If keyboard input is not enabled, this message should be ignored
                    if( !m_bKeyboardInput )
                        return false;

                    switch( wParam )
                    {
                        case VK_RIGHT:
                        case VK_DOWN:
                            if( s_pControlFocus != NULL )
                            {
                                return OnCycleFocus( true );
                            }
                            break;

                        case VK_LEFT:
                        case VK_UP:
                            if( s_pControlFocus != NULL )
                            {
                                return OnCycleFocus( false );
                            }
                            break;

                        case VK_TAB:
                        {
                            bool bShiftDown = ( ( GetKeyState( VK_SHIFT ) & 0x8000 ) != 0 );
                            return OnCycleFocus( !bShiftDown );
                        }
                    }
                }

                break;
            }


            // Mouse messages
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
        case WM_XBUTTONDBLCLK:
        case WM_MOUSEWHEEL:
            {
                // If not accepting mouse input, return false to indicate the message should still 
                // be handled by the application (usually to move the camera).
                if( !m_bMouseInput )
                    return false;

                POINT mousePoint =
                {
                    short( LOWORD( lParam ) ), short( HIWORD( lParam ) )
                };
                mousePoint.x -= m_x;
                mousePoint.y -= m_y;

                // If caption is enabled, offset the Y coordinate by the negative of its height.
                if( m_bCaption )
                    mousePoint.y -= m_nCaptionHeight;

                // If a control is in focus, it belongs to this dialog, and it's enabled, then give
                // it the first chance at handling the message.
                if( s_pControlFocus &&
                    s_pControlFocus->m_pDialog == this &&
                    s_pControlFocus->GetEnabled() )
                {
                    if( s_pControlFocus->HandleMouse( uMsg, mousePoint, wParam, lParam ) )
                        return true;
                }

                // Not yet handled, see if the mouse is over any controls
                CDXUTControl* pControl = GetControlAtPoint( mousePoint );
                if( pControl != NULL && pControl->GetEnabled() )
                {
                    bHandled = pControl->HandleMouse( uMsg, mousePoint, wParam, lParam );
                    if( bHandled )
                        return true;
                }
                else
                {
                    // Mouse not over any controls in this dialog, if there was a control
                    // which had focus it just lost it
                    if( uMsg == WM_LBUTTONDOWN &&
                        s_pControlFocus &&
                        s_pControlFocus->m_pDialog == this )
                    {
                        s_pControlFocus->OnFocusOut();
                        s_pControlFocus = NULL;
                    }
                }

                // Still not handled, hand this off to the dialog. Return false to indicate the
                // message should still be handled by the application (usually to move the camera).
                switch( uMsg )
                {
                    case WM_MOUSEMOVE:
                        OnMouseMove( mousePoint );
                        return false;
                }

                break;
            }

        case WM_CAPTURECHANGED:
        {
            // The application has lost mouse capture.
            // The dialog object may not have received
            // a WM_MOUSEUP when capture changed. Reset
            // m_bDrag so that the dialog does not mistakenly
            // think the mouse button is still held down.
            if( ( HWND )lParam != hWnd )
                m_bDrag = false;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------
CDXUTControl* CDXUTDialog::GetControlAtPoint( POINT pt )
{
    // Search through all child controls for the first one which
    // contains the mouse point
    for( int i = 0; i < m_Controls.GetSize(); i++ )
    {
        CDXUTControl* pControl = m_Controls.GetAt( i );

        if( pControl == NULL )
        {
            continue;
        }

        // We only return the current control if it is visible
        // and enabled.  Because GetControlAtPoint() is used to do mouse
        // hittest, it makes sense to perform this filtering.
        if( pControl->ContainsPoint( pt ) && pControl->GetEnabled() && pControl->GetVisible() )
        {
            return pControl;
        }
    }

    return NULL;
}


//--------------------------------------------------------------------------------------
bool CDXUTDialog::GetControlEnabled( int ID )
{
    CDXUTControl* pControl = GetControl( ID );
    if( pControl == NULL )
        return false;

    return pControl->GetEnabled();
}



//--------------------------------------------------------------------------------------
void CDXUTDialog::SetControlEnabled( int ID, bool bEnabled )
{
    CDXUTControl* pControl = GetControl( ID );
    if( pControl == NULL )
        return;

    pControl->SetEnabled( bEnabled );
}


//--------------------------------------------------------------------------------------
void CDXUTDialog::OnMouseUp( POINT pt )
{
    s_pControlPressed = NULL;
    m_pControlMouseOver = NULL;
}


//--------------------------------------------------------------------------------------
void CDXUTDialog::OnMouseMove( POINT pt )
{
    // Figure out which control the mouse is over now
    CDXUTControl* pControl = GetControlAtPoint( pt );

    // If the mouse is still over the same control, nothing needs to be done
    if( pControl == m_pControlMouseOver )
        return;

    // Handle mouse leaving the old control
    if( m_pControlMouseOver )
        m_pControlMouseOver->OnMouseLeave();

    // Handle mouse entering the new control
    m_pControlMouseOver = pControl;
    if( pControl != NULL )
        m_pControlMouseOver->OnMouseEnter();
}


//--------------------------------------------------------------------------------------
HRESULT CDXUTDialog::SetDefaultElement( UINT nControlType, UINT iElement, CDXUTElement* pElement )
{
    // If this Element type already exist in the list, simply update the stored Element
    for( int i = 0; i < m_DefaultElements.GetSize(); i++ )
    {
        DXUTElementHolder* pElementHolder = m_DefaultElements.GetAt( i );

        if( pElementHolder->nControlType == nControlType &&
            pElementHolder->iElement == iElement )
        {
            pElementHolder->Element = *pElement;
            return S_OK;
        }
    }

    // Otherwise, add a new entry
    DXUTElementHolder* pNewHolder;
    pNewHolder = new DXUTElementHolder;
    if( pNewHolder == NULL )
        return E_OUTOFMEMORY;

    pNewHolder->nControlType = nControlType;
    pNewHolder->iElement = iElement;
    pNewHolder->Element = *pElement;

    HRESULT hr = m_DefaultElements.Add( pNewHolder );
    if( FAILED( hr ) )
    {
        delete pNewHolder;
    }
    return hr;
}


//--------------------------------------------------------------------------------------
CDXUTElement* CDXUTDialog::GetDefaultElement( UINT nControlType, UINT iElement )
{
    for( int i = 0; i < m_DefaultElements.GetSize(); i++ )
    {
        DXUTElementHolder* pElementHolder = m_DefaultElements.GetAt( i );

        if( pElementHolder->nControlType == nControlType &&
            pElementHolder->iElement == iElement )
        {
            return &pElementHolder->Element;
        }
    }

    return NULL;
}

//--------------------------------------------------------------------------------------
HRESULT CDXUTDialog::AddButton( int ID, LPCWSTR strText, int x, int y, int width, int height, UINT nHotkey,
                                bool bIsDefault, CDXUTButton** ppCreated )
{
    HRESULT hr = S_OK;

    CDXUTButton* pButton = new CDXUTButton( this );

    if( ppCreated != NULL )
        *ppCreated = pButton;

    if( pButton == NULL )
        return E_OUTOFMEMORY;

    hr = AddControl( pButton );
    if( FAILED( hr ) )
        return hr;

    // Set the ID and list index
    pButton->SetID( ID );
    pButton->SetLocation( x, y );
    pButton->SetSize( width, height );
    pButton->SetHotkey( nHotkey );
    pButton->m_bIsDefault = bIsDefault;

    return S_OK;
}

//--------------------------------------------------------------------------------------
HRESULT CDXUTDialog::InitControl( CDXUTControl* pControl )
{
    HRESULT hr;

    if( pControl == NULL )
        return E_INVALIDARG;

    pControl->m_Index = m_Controls.GetSize();

    // Look for a default Element entries
    for( int i = 0; i < m_DefaultElements.GetSize(); i++ )
    {
        DXUTElementHolder* pElementHolder = m_DefaultElements.GetAt( i );
        if( pElementHolder->nControlType == pControl->GetType() )
            pControl->SetElement( pElementHolder->iElement, &pElementHolder->Element );
    }

    V_RETURN( pControl->OnInit() );

    return S_OK;
}



//--------------------------------------------------------------------------------------
HRESULT CDXUTDialog::AddControl( CDXUTControl* pControl )
{
    HRESULT hr = S_OK;

    hr = InitControl( pControl );
    if( FAILED( hr ) )
        return DXTRACE_ERR( L"CDXUTDialog::InitControl", hr );

    // Add to the list
    hr = m_Controls.Add( pControl );
    if( FAILED( hr ) )
    {
        return DXTRACE_ERR( L"CGrowableArray::Add", hr );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
CDXUTControl* CDXUTDialog::GetControl( int ID )
{
    // Try to find the control with the given ID
    for( int i = 0; i < m_Controls.GetSize(); i++ )
    {
        CDXUTControl* pControl = m_Controls.GetAt( i );

        if( pControl->GetID() == ID )
        {
            return pControl;
        }
    }

    // Not found
    return NULL;
}



//--------------------------------------------------------------------------------------
CDXUTControl* CDXUTDialog::GetControl( int ID, UINT nControlType )
{
    // Try to find the control with the given ID
    for( int i = 0; i < m_Controls.GetSize(); i++ )
    {
        CDXUTControl* pControl = m_Controls.GetAt( i );

        if( pControl->GetID() == ID && pControl->GetType() == nControlType )
        {
            return pControl;
        }
    }

    // Not found
    return NULL;
}



//--------------------------------------------------------------------------------------
CDXUTControl* CDXUTDialog::GetNextControl( CDXUTControl* pControl )
{
    int index = pControl->m_Index + 1;

    CDXUTDialog* pDialog = pControl->m_pDialog;

    // Cycle through dialogs in the loop to find the next control. Note
    // that if only one control exists in all looped dialogs it will
    // be the returned 'next' control.
    while( index >= ( int )pDialog->m_Controls.GetSize() )
    {
        pDialog = pDialog->m_pNextDialog;
        index = 0;
    }

    return pDialog->m_Controls.GetAt( index );
}

//--------------------------------------------------------------------------------------
CDXUTControl* CDXUTDialog::GetPrevControl( CDXUTControl* pControl )
{
    int index = pControl->m_Index - 1;

    CDXUTDialog* pDialog = pControl->m_pDialog;

    // Cycle through dialogs in the loop to find the next control. Note
    // that if only one control exists in all looped dialogs it will
    // be the returned 'previous' control.
    while( index < 0 )
    {
        pDialog = pDialog->m_pPrevDialog;
        if( pDialog == NULL )
            pDialog = pControl->m_pDialog;

        index = pDialog->m_Controls.GetSize() - 1;
    }

    return pDialog->m_Controls.GetAt( index );
}

//--------------------------------------------------------------------------------------
void CDXUTDialog::RequestFocus( CDXUTControl* pControl )
{
    if( s_pControlFocus == pControl )
        return;

    if( !pControl->CanHaveFocus() )
        return;

    if( s_pControlFocus )
        s_pControlFocus->OnFocusOut();

    pControl->OnFocusIn();
    s_pControlFocus = pControl;
}

//--------------------------------------------------------------------------------------
HRESULT CDXUTDialog::DrawSprite( CDXUTElement* pElement, RECT* prcDest, float fDepth )
{
    return DrawSprite11( pElement, prcDest, fDepth );
}

//--------------------------------------------------------------------------------------
HRESULT CDXUTDialog::DrawSprite11( CDXUTElement* pElement, RECT* prcDest, float fDepth )
{
    // No need to draw fully transparent layers
    if( pElement->TextureColor.Current.a == 0 )
        return S_OK;

    RECT rcTexture = pElement->rcTexture;

    RECT rcScreen = *prcDest;
    OffsetRect( &rcScreen, m_x, m_y );

    // If caption is enabled, offset the Y position by its height.
    if( m_bCaption )
        OffsetRect( &rcScreen, 0, m_nCaptionHeight );

    DXUTTextureNode* pTextureNode = GetTexture( pElement->iTexture );
    if( pTextureNode == NULL )
        return E_FAIL;

    float fBBWidth = ( float )m_pManager->m_nBackBufferWidth;
    float fBBHeight = ( float )m_pManager->m_nBackBufferHeight;
    float fTexWidth = ( float )pTextureNode->dwWidth;
    float fTexHeight = ( float )pTextureNode->dwHeight;

    float fRectLeft = rcScreen.left / fBBWidth;
    float fRectTop = 1.0f - rcScreen.top / fBBHeight;
    float fRectRight = rcScreen.right / fBBWidth;
    float fRectBottom = 1.0f - rcScreen.bottom / fBBHeight;

    fRectLeft = fRectLeft * 2.0f - 1.0f;
    fRectTop = fRectTop * 2.0f - 1.0f;
    fRectRight = fRectRight * 2.0f - 1.0f;
    fRectBottom = fRectBottom * 2.0f - 1.0f;
    
    float fTexLeft = rcTexture.left / fTexWidth;
    float fTexTop = rcTexture.top / fTexHeight;
    float fTexRight = rcTexture.right / fTexWidth;
    float fTexBottom = rcTexture.bottom / fTexHeight;

    // Add 6 sprite vertices
    DXUTSpriteVertex SpriteVertex;

    // tri1
    SpriteVertex.vPos = D3DXVECTOR3( fRectLeft, fRectTop, fDepth );
    SpriteVertex.vTex = D3DXVECTOR2( fTexLeft, fTexTop );
    SpriteVertex.vColor = pElement->TextureColor.Current;
    m_pManager->m_SpriteVertices.Add( SpriteVertex );

    SpriteVertex.vPos = D3DXVECTOR3( fRectRight, fRectTop, fDepth );
    SpriteVertex.vTex = D3DXVECTOR2( fTexRight, fTexTop );
    SpriteVertex.vColor = pElement->TextureColor.Current;
    m_pManager->m_SpriteVertices.Add( SpriteVertex );

    SpriteVertex.vPos = D3DXVECTOR3( fRectLeft, fRectBottom, fDepth );
    SpriteVertex.vTex = D3DXVECTOR2( fTexLeft, fTexBottom );
    SpriteVertex.vColor = pElement->TextureColor.Current;
    m_pManager->m_SpriteVertices.Add( SpriteVertex );

    // tri2
    SpriteVertex.vPos = D3DXVECTOR3( fRectRight, fRectTop, fDepth );
    SpriteVertex.vTex = D3DXVECTOR2( fTexRight, fTexTop );
    SpriteVertex.vColor = pElement->TextureColor.Current;
    m_pManager->m_SpriteVertices.Add( SpriteVertex );

    SpriteVertex.vPos = D3DXVECTOR3( fRectRight, fRectBottom, fDepth );
    SpriteVertex.vTex = D3DXVECTOR2( fTexRight, fTexBottom );
    SpriteVertex.vColor = pElement->TextureColor.Current;
    m_pManager->m_SpriteVertices.Add( SpriteVertex );

    SpriteVertex.vPos = D3DXVECTOR3( fRectLeft, fRectBottom, fDepth );
    SpriteVertex.vTex = D3DXVECTOR2( fTexLeft, fTexBottom );
    SpriteVertex.vColor = pElement->TextureColor.Current;
    m_pManager->m_SpriteVertices.Add( SpriteVertex );

    // Why are we drawing the sprite every time?  This is very inefficient, but the sprite workaround doesn't have support for sorting now, so we have to
    // draw a sprite every time to keep the order correct between sprites and text.
    m_pManager->EndSprites11( DXUTGetD3D11Device(), DXUTGetD3D11DeviceContext() );

    return S_OK;
}

ID3D11InputLayout* g_pInputLayout11 = NULL;

//--------------------------------------------------------------------------------------
void CDXUTDialog::SetBackgroundColors( D3DCOLOR colorTopLeft, D3DCOLOR colorTopRight, D3DCOLOR colorBottomLeft,
                                       D3DCOLOR colorBottomRight )
{
    m_colorTopLeft = colorTopLeft;
    m_colorTopRight = colorTopRight;
    m_colorBottomLeft = colorBottomLeft;
    m_colorBottomRight = colorBottomRight;
}


//--------------------------------------------------------------------------------------
void CDXUTDialog::SetNextDialog( CDXUTDialog* pNextDialog )
{
    if( pNextDialog == NULL )
        pNextDialog = this;

    m_pNextDialog = pNextDialog;
    if( pNextDialog )
        m_pNextDialog->m_pPrevDialog = this;
}


//--------------------------------------------------------------------------------------
void CDXUTDialog::ClearFocus()
{
    if( s_pControlFocus )
    {
        s_pControlFocus->OnFocusOut();
        s_pControlFocus = NULL;
    }

    ReleaseCapture();
}


//--------------------------------------------------------------------------------------
void CDXUTDialog::FocusDefaultControl()
{
    // Check for default control in this dialog
    for( int i = 0; i < m_Controls.GetSize(); i++ )
    {
        CDXUTControl* pControl = m_Controls.GetAt( i );
        if( pControl->m_bIsDefault )
        {
            // Remove focus from the current control
            ClearFocus();

            // Give focus to the default control
            s_pControlFocus = pControl;
            s_pControlFocus->OnFocusIn();
            return;
        }
    }
}


//--------------------------------------------------------------------------------------
bool CDXUTDialog::OnCycleFocus( bool bForward )
{
    CDXUTControl* pControl = NULL;
    CDXUTDialog* pDialog = NULL; // pDialog and pLastDialog are used to track wrapping of
    CDXUTDialog* pLastDialog;    // focus from first control to last or vice versa.

    if( s_pControlFocus == NULL )
    {
        // If s_pControlFocus is NULL, we focus the first control of first dialog in
        // the case that bForward is true, and focus the last control of last dialog when
        // bForward is false.
        //
        if( bForward )
        {
            // Search for the first control from the start of the dialog
            // array.
            for( int d = 0; d < m_pManager->m_Dialogs.GetSize(); ++d )
            {
                pDialog = pLastDialog = m_pManager->m_Dialogs.GetAt( d );
                if( pDialog && pDialog->m_Controls.GetSize() > 0 )
                {
                    pControl = pDialog->m_Controls.GetAt( 0 );
                    break;
                }
            }

            if( !pDialog || !pControl )
            {
                // No dialog has been registered yet or no controls have been
                // added to the dialogs. Cannot proceed.
                return true;
            }
        }
        else
        {
            // Search for the first control from the end of the dialog
            // array.
            for( int d = m_pManager->m_Dialogs.GetSize() - 1; d >= 0; --d )
            {
                pDialog = pLastDialog = m_pManager->m_Dialogs.GetAt( d );
                if( pDialog && pDialog->m_Controls.GetSize() > 0 )
                {
                    pControl = pDialog->m_Controls.GetAt( pDialog->m_Controls.GetSize() - 1 );
                    break;
                }
            }

            if( !pDialog || !pControl )
            {
                // No dialog has been registered yet or no controls have been
                // added to the dialogs. Cannot proceed.
                return true;
            }
        }
    }
    else if( s_pControlFocus->m_pDialog != this )
    {
        // If a control belonging to another dialog has focus, let that other
        // dialog handle this event by returning false.
        //
        return false;
    }
    else
    {
        // Focused control belongs to this dialog. Cycle to the
        // next/previous control.
        pLastDialog = s_pControlFocus->m_pDialog;
        pControl = ( bForward ) ? GetNextControl( s_pControlFocus ) : GetPrevControl( s_pControlFocus );
        pDialog = pControl->m_pDialog;
    }

    for( int i = 0; i < 0xffff; i++ )
    {
        // If we just wrapped from last control to first or vice versa,
        // set the focused control to NULL. This state, where no control
        // has focus, allows the camera to work.
        int nLastDialogIndex = m_pManager->m_Dialogs.IndexOf( pLastDialog );
        int nDialogIndex = m_pManager->m_Dialogs.IndexOf( pDialog );
        if( ( !bForward && nLastDialogIndex < nDialogIndex ) ||
            ( bForward && nDialogIndex < nLastDialogIndex ) )
        {
            if( s_pControlFocus )
                s_pControlFocus->OnFocusOut();
            s_pControlFocus = NULL;
            return true;
        }

        // If we've gone in a full circle then focus doesn't change
        if( pControl == s_pControlFocus )
            return true;

        // If the dialog accepts keybord input and the control can have focus then
        // move focus
        if( pControl->m_pDialog->m_bKeyboardInput && pControl->CanHaveFocus() )
        {
            if( s_pControlFocus )
                s_pControlFocus->OnFocusOut();
            s_pControlFocus = pControl;
            s_pControlFocus->OnFocusIn();
            return true;
        }

        pLastDialog = pDialog;
        pControl = ( bForward ) ? GetNextControl( pControl ) : GetPrevControl( pControl );
        pDialog = pControl->m_pDialog;
    }

    // If we reached this point, the chain of dialogs didn't form a complete loop
    DXTRACE_ERR( L"CDXUTDialog: Multiple dialogs are improperly chained together", E_FAIL );
    return false;
}

//--------------------------------------------------------------------------------------
HRESULT CDXUTDialogResourceManager::CreateFont11( UINT iFont )
{
    return S_OK;
}

//--------------------------------------------------------------------------------------
HRESULT CDXUTDialogResourceManager::CreateTexture11( UINT iTexture )
{
    HRESULT hr = S_OK;

    DXUTTextureNode* pTextureNode = m_TextureCache.GetAt( iTexture );

    if( !pTextureNode->bFileSource )
    {
        if( pTextureNode->nResourceID == 0xFFFF && pTextureNode->hResourceModule == ( HMODULE )0xFFFF )
        {
            hr = DXUTCreateGUITextureFromInternalArray11( m_pd3d11Device, &pTextureNode->pTexture11, NULL );
            if( FAILED( hr ) )
                return DXTRACE_ERR( L"D3DX11CreateResourceFromFileInMemory", hr );
        }
        //else
        //{
        //    LPCWSTR pID = pTextureNode->nResourceID ? ( LPCWSTR )( size_t )pTextureNode->nResourceID :
        //        pTextureNode->strFilename;

        //    D3DX10_IMAGE_INFO SrcInfo;
        //    D3DX10GetImageInfoFromResource( NULL, pID, NULL, &SrcInfo, NULL );

        //    // Create texture from resource
        //    ID3D10Resource* pRes;
        //    D3DX10_IMAGE_LOAD_INFO loadInfo;
        //    loadInfo.Width = D3DX10_DEFAULT;
        //    loadInfo.Height = D3DX10_DEFAULT;
        //    loadInfo.Depth = D3DX10_DEFAULT;
        //    loadInfo.FirstMipLevel = 0;
        //    loadInfo.MipLevels = 1;
        //    loadInfo.Usage = D3D10_USAGE_DEFAULT;
        //    loadInfo.BindFlags = D3D10_BIND_SHADER_RESOURCE;
        //    loadInfo.CpuAccessFlags = 0;
        //    loadInfo.MiscFlags = 0;
        //    loadInfo.Format = MAKE_TYPELESS( SrcInfo.Format );
        //    loadInfo.Filter = D3DX10_FILTER_NONE;
        //    loadInfo.MipFilter = D3DX10_FILTER_NONE;
        //    loadInfo.pSrcInfo = &SrcInfo;

        //    hr = D3DX10CreateTextureFromResource( m_pd3d10Device, pTextureNode->hResourceModule, pID, &loadInfo,
        //                                          NULL, &pRes, NULL );
        //    if( FAILED( hr ) )
        //        return DXTRACE_ERR( L"D3DX10CreateResourceFromResource", hr );
        //    hr = pRes->QueryInterface( __uuidof( ID3D10Texture2D ), ( LPVOID* )&pTextureNode->pTexture10 );
        //    SAFE_RELEASE( pRes );
        //    if( FAILED( hr ) )
        //        return hr;
        //}
    }
    else
    {
        //
        //// Make sure there's a texture to create
        //if( pTextureNode->strFilename[0] == 0 )
        //    return S_OK;

        //D3DX10_IMAGE_INFO SrcInfo;
        //D3DX10GetImageInfoFromFile( pTextureNode->strFilename, NULL, &SrcInfo, NULL );

        //// Create texture from file
        //ID3D10Resource* pRes;
        //D3DX10_IMAGE_LOAD_INFO loadInfo;
        //loadInfo.Width = D3DX10_DEFAULT;
        //loadInfo.Height = D3DX10_DEFAULT;
        //loadInfo.Depth = D3DX10_DEFAULT;
        //loadInfo.FirstMipLevel = 0;
        //loadInfo.MipLevels = 1;
        //loadInfo.Usage = D3D10_USAGE_DEFAULT;
        //loadInfo.BindFlags = D3D10_BIND_SHADER_RESOURCE;
        //loadInfo.CpuAccessFlags = 0;
        //loadInfo.MiscFlags = 0;
        //loadInfo.Format = MAKE_TYPELESS( SrcInfo.Format );
        //loadInfo.Filter = D3DX10_FILTER_NONE;
        //loadInfo.MipFilter = D3DX10_FILTER_NONE;
        //loadInfo.pSrcInfo = &SrcInfo;
        //hr = D3DX10CreateTextureFromFile( m_pd3d10Device, pTextureNode->strFilename, &loadInfo, NULL, &pRes, NULL );
        //if( FAILED( hr ) )
        //{
        //    return DXTRACE_ERR( L"D3DX10CreateResourceFromFileEx", hr );
        //}
        //hr = pRes->QueryInterface( __uuidof( ID3D10Texture2D ), ( LPVOID* )&pTextureNode->pTexture10 );
        //SAFE_RELEASE( pRes );
        //if( FAILED( hr ) )
        //    return hr;
        //
    }

    // Store dimensions
    D3D11_TEXTURE2D_DESC desc;
    pTextureNode->pTexture11->GetDesc( &desc );
    pTextureNode->dwWidth = desc.Width;
    pTextureNode->dwHeight = desc.Height;

    // Create resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SRVDesc.Texture2D.MipLevels = 1;
    SRVDesc.Texture2D.MostDetailedMip = 0;
    hr = m_pd3d11Device->CreateShaderResourceView( pTextureNode->pTexture11, &SRVDesc, &pTextureNode->pTexResView11 );

    return hr;
}


//--------------------------------------------------------------------------------------
void CDXUTDialog::InitDefaultElements()
{
    CDXUTElement Element;
    RECT rcTexture;

    //-------------------------------------
    // Element for the caption
    //-------------------------------------
    SetRect( &rcTexture, 17, 269, 241, 287 );
    m_CapElement.SetTexture( 0, &rcTexture );
    m_CapElement.TextureColor.States[ DXUT_STATE_NORMAL ] = D3DCOLOR_ARGB( 255, 255, 255, 255 );
    // Pre-blend as we don't need to transition the state
    m_CapElement.TextureColor.Blend( DXUT_STATE_NORMAL, 10.0f );
    //-------------------------------------
    // CDXUTStatic
    //-------------------------------------

    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_STATIC, 0, &Element );


    //-------------------------------------
    // CDXUTButton - Button
    //-------------------------------------
    SetRect( &rcTexture, 0, 0, 136, 54 );
    Element.SetTexture( 0, &rcTexture );
    Element.TextureColor.States[ DXUT_STATE_NORMAL ] = D3DCOLOR_ARGB( 150, 255, 255, 255 );
    Element.TextureColor.States[ DXUT_STATE_PRESSED ] = D3DCOLOR_ARGB( 200, 255, 255, 255 );
    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_BUTTON, 0, &Element );


    //-------------------------------------
    // CDXUTButton - Fill layer
    //-------------------------------------
    SetRect( &rcTexture, 136, 0, 252, 54 );
    Element.SetTexture( 0, &rcTexture, D3DCOLOR_ARGB( 0, 255, 255, 255 ) );
    Element.TextureColor.States[ DXUT_STATE_MOUSEOVER ] = D3DCOLOR_ARGB( 160, 255, 255, 255 );
    Element.TextureColor.States[ DXUT_STATE_PRESSED ] = D3DCOLOR_ARGB( 60, 0, 0, 0 );
    Element.TextureColor.States[ DXUT_STATE_FOCUS ] = D3DCOLOR_ARGB( 30, 255, 255, 255 );


    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_BUTTON, 1, &Element );


    //-------------------------------------
    // CDXUTCheckBox - Box
    //-------------------------------------
    SetRect( &rcTexture, 0, 54, 27, 81 );
    Element.SetTexture( 0, &rcTexture );
    Element.TextureColor.States[ DXUT_STATE_NORMAL ] = D3DCOLOR_ARGB( 150, 255, 255, 255 );
    Element.TextureColor.States[ DXUT_STATE_FOCUS ] = D3DCOLOR_ARGB( 200, 255, 255, 255 );
    Element.TextureColor.States[ DXUT_STATE_PRESSED ] = D3DCOLOR_ARGB( 255, 255, 255, 255 );

    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_CHECKBOX, 0, &Element );


    //-------------------------------------
    // CDXUTCheckBox - Check
    //-------------------------------------
    SetRect( &rcTexture, 27, 54, 54, 81 );
    Element.SetTexture( 0, &rcTexture );

    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_CHECKBOX, 1, &Element );


    //-------------------------------------
    // CDXUTRadioButton - Box
    //-------------------------------------
    SetRect( &rcTexture, 54, 54, 81, 81 );
    Element.SetTexture( 0, &rcTexture );
    Element.TextureColor.States[ DXUT_STATE_NORMAL ] = D3DCOLOR_ARGB( 150, 255, 255, 255 );
    Element.TextureColor.States[ DXUT_STATE_FOCUS ] = D3DCOLOR_ARGB( 200, 255, 255, 255 );
    Element.TextureColor.States[ DXUT_STATE_PRESSED ] = D3DCOLOR_ARGB( 255, 255, 255, 255 );

    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_RADIOBUTTON, 0, &Element );


    //-------------------------------------
    // CDXUTRadioButton - Check
    //-------------------------------------
    SetRect( &rcTexture, 81, 54, 108, 81 );
    Element.SetTexture( 0, &rcTexture );

    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_RADIOBUTTON, 1, &Element );


    //-------------------------------------
    // CDXUTComboBox - Main
    //-------------------------------------
    SetRect( &rcTexture, 7, 81, 247, 123 );
    Element.SetTexture( 0, &rcTexture );
    Element.TextureColor.States[ DXUT_STATE_NORMAL ] = D3DCOLOR_ARGB( 150, 200, 200, 200 );
    Element.TextureColor.States[ DXUT_STATE_FOCUS ] = D3DCOLOR_ARGB( 170, 230, 230, 230 );
    Element.TextureColor.States[ DXUT_STATE_DISABLED ] = D3DCOLOR_ARGB( 70, 200, 200, 200 );


    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_COMBOBOX, 0, &Element );


    //-------------------------------------
    // CDXUTComboBox - Button
    //-------------------------------------
    SetRect( &rcTexture, 98, 189, 151, 238 );
    Element.SetTexture( 0, &rcTexture );
    Element.TextureColor.States[ DXUT_STATE_NORMAL ] = D3DCOLOR_ARGB( 150, 255, 255, 255 );
    Element.TextureColor.States[ DXUT_STATE_PRESSED ] = D3DCOLOR_ARGB( 255, 150, 150, 150 );
    Element.TextureColor.States[ DXUT_STATE_FOCUS ] = D3DCOLOR_ARGB( 200, 255, 255, 255 );
    Element.TextureColor.States[ DXUT_STATE_DISABLED ] = D3DCOLOR_ARGB( 70, 255, 255, 255 );

    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_COMBOBOX, 1, &Element );


    //-------------------------------------
    // CDXUTComboBox - Dropdown
    //-------------------------------------
    SetRect( &rcTexture, 13, 123, 241, 160 );
    Element.SetTexture( 0, &rcTexture );
    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_COMBOBOX, 2, &Element );


    //-------------------------------------
    // CDXUTComboBox - Selection
    //-------------------------------------
    SetRect( &rcTexture, 12, 163, 239, 183 );
    Element.SetTexture( 0, &rcTexture );
    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_COMBOBOX, 3, &Element );


    //-------------------------------------
    // CDXUTSlider - Track
    //-------------------------------------
    SetRect( &rcTexture, 1, 187, 93, 228 );
    Element.SetTexture( 0, &rcTexture );
    Element.TextureColor.States[ DXUT_STATE_NORMAL ] = D3DCOLOR_ARGB( 150, 255, 255, 255 );
    Element.TextureColor.States[ DXUT_STATE_FOCUS ] = D3DCOLOR_ARGB( 200, 255, 255, 255 );
    Element.TextureColor.States[ DXUT_STATE_DISABLED ] = D3DCOLOR_ARGB( 70, 255, 255, 255 );

    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_SLIDER, 0, &Element );

    //-------------------------------------
    // CDXUTSlider - Button
    //-------------------------------------
    SetRect( &rcTexture, 151, 193, 192, 234 );
    Element.SetTexture( 0, &rcTexture );

    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_SLIDER, 1, &Element );

    //-------------------------------------
    // CDXUTScrollBar - Track
    //-------------------------------------
    int nScrollBarStartX = 196;
    int nScrollBarStartY = 191;
    SetRect( &rcTexture, nScrollBarStartX + 0, nScrollBarStartY + 21, nScrollBarStartX + 22, nScrollBarStartY + 32 );
    Element.SetTexture( 0, &rcTexture );
    Element.TextureColor.States[ DXUT_STATE_DISABLED ] = D3DCOLOR_ARGB( 255, 200, 200, 200 );

    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_SCROLLBAR, 0, &Element );

    //-------------------------------------
    // CDXUTScrollBar - Up Arrow
    //-------------------------------------
    SetRect( &rcTexture, nScrollBarStartX + 0, nScrollBarStartY + 1, nScrollBarStartX + 22, nScrollBarStartY + 21 );
    Element.SetTexture( 0, &rcTexture );
    Element.TextureColor.States[ DXUT_STATE_DISABLED ] = D3DCOLOR_ARGB( 255, 200, 200, 200 );


    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_SCROLLBAR, 1, &Element );

    //-------------------------------------
    // CDXUTScrollBar - Down Arrow
    //-------------------------------------
    SetRect( &rcTexture, nScrollBarStartX + 0, nScrollBarStartY + 32, nScrollBarStartX + 22, nScrollBarStartY + 53 );
    Element.SetTexture( 0, &rcTexture );
    Element.TextureColor.States[ DXUT_STATE_DISABLED ] = D3DCOLOR_ARGB( 255, 200, 200, 200 );


    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_SCROLLBAR, 2, &Element );

    //-------------------------------------
    // CDXUTScrollBar - Button
    //-------------------------------------
    SetRect( &rcTexture, 220, 192, 238, 234 );
    Element.SetTexture( 0, &rcTexture );

    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_SCROLLBAR, 3, &Element );


    //-------------------------------------
    // CDXUTEditBox
    //-------------------------------------
    // Element assignment:
    //   0 - text area
    //   1 - top left border
    //   2 - top border
    //   3 - top right border
    //   4 - left border
    //   5 - right border
    //   6 - lower left border
    //   7 - lower border
    //   8 - lower right border

    // Assign the style
    SetRect( &rcTexture, 14, 90, 241, 113 );
    Element.SetTexture( 0, &rcTexture );
    SetDefaultElement( DXUT_CONTROL_EDITBOX, 0, &Element );
    SetRect( &rcTexture, 8, 82, 14, 90 );
    Element.SetTexture( 0, &rcTexture );
    SetDefaultElement( DXUT_CONTROL_EDITBOX, 1, &Element );
    SetRect( &rcTexture, 14, 82, 241, 90 );
    Element.SetTexture( 0, &rcTexture );
    SetDefaultElement( DXUT_CONTROL_EDITBOX, 2, &Element );
    SetRect( &rcTexture, 241, 82, 246, 90 );
    Element.SetTexture( 0, &rcTexture );
    SetDefaultElement( DXUT_CONTROL_EDITBOX, 3, &Element );
    SetRect( &rcTexture, 8, 90, 14, 113 );
    Element.SetTexture( 0, &rcTexture );
    SetDefaultElement( DXUT_CONTROL_EDITBOX, 4, &Element );
    SetRect( &rcTexture, 241, 90, 246, 113 );
    Element.SetTexture( 0, &rcTexture );
    SetDefaultElement( DXUT_CONTROL_EDITBOX, 5, &Element );
    SetRect( &rcTexture, 8, 113, 14, 121 );
    Element.SetTexture( 0, &rcTexture );
    SetDefaultElement( DXUT_CONTROL_EDITBOX, 6, &Element );
    SetRect( &rcTexture, 14, 113, 241, 121 );
    Element.SetTexture( 0, &rcTexture );
    SetDefaultElement( DXUT_CONTROL_EDITBOX, 7, &Element );
    SetRect( &rcTexture, 241, 113, 246, 121 );
    Element.SetTexture( 0, &rcTexture );
    SetDefaultElement( DXUT_CONTROL_EDITBOX, 8, &Element );

    //-------------------------------------
    // CDXUTListBox - Main
    //-------------------------------------
    SetRect( &rcTexture, 13, 123, 241, 160 );
    Element.SetTexture( 0, &rcTexture );

    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_LISTBOX, 0, &Element );

    //-------------------------------------
    // CDXUTListBox - Selection
    //-------------------------------------

    SetRect( &rcTexture, 16, 166, 240, 183 );
    Element.SetTexture( 0, &rcTexture );

    // Assign the Element
    SetDefaultElement( DXUT_CONTROL_LISTBOX, 1, &Element );
}



//--------------------------------------------------------------------------------------
// CDXUTControl class
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
CDXUTControl::CDXUTControl( CDXUTDialog* pDialog )
{
    m_Type = DXUT_CONTROL_BUTTON;
    m_pDialog = pDialog;
    m_ID = 0;
    m_Index = 0;
    m_pUserData = NULL;

    m_bEnabled = true;
    m_bVisible = true;
    m_bMouseOver = false;
    m_bHasFocus = false;
    m_bIsDefault = false;

    m_pDialog = NULL;

    m_x = 0;
    m_y = 0;
    m_width = 0;
    m_height = 0;

    ZeroMemory( &m_rcBoundingBox, sizeof( m_rcBoundingBox ) );
}


CDXUTControl::~CDXUTControl()
{
    for( int i = 0; i < m_Elements.GetSize(); ++i )
    {
        delete m_Elements[i];
    }
    m_Elements.RemoveAll();
}

//--------------------------------------------------------------------------------------
HRESULT CDXUTControl::SetElement( UINT iElement, CDXUTElement* pElement )
{
    HRESULT hr = S_OK;

    if( pElement == NULL )
        return E_INVALIDARG;

    // Make certain the array is this large
    for( UINT i = m_Elements.GetSize(); i <= iElement; i++ )
    {
        CDXUTElement* pNewElement = new CDXUTElement();
        if( pNewElement == NULL )
            return E_OUTOFMEMORY;

        hr = m_Elements.Add( pNewElement );
        if( FAILED( hr ) )
        {
            SAFE_DELETE( pNewElement );
            return hr;
        }
    }

    // Update the data
    CDXUTElement* pCurElement = m_Elements.GetAt( iElement );
    *pCurElement = *pElement;

    return S_OK;
}


//--------------------------------------------------------------------------------------
void CDXUTControl::Refresh()
{
    m_bMouseOver = false;
    m_bHasFocus = false;

    for( int i = 0; i < m_Elements.GetSize(); i++ )
    {
        CDXUTElement* pElement = m_Elements.GetAt( i );
        pElement->Refresh();
    }
}


//--------------------------------------------------------------------------------------
void CDXUTControl::UpdateRects()
{
    SetRect( &m_rcBoundingBox, m_x, m_y, m_x + m_width, m_y + m_height );
}

//--------------------------------------------------------------------------------------
CDXUTButton::CDXUTButton( CDXUTDialog* pDialog )
{
    m_Type = DXUT_CONTROL_BUTTON;
    m_pDialog = pDialog;

    m_bPressed = false;
    m_nHotkey = 0;
}

//--------------------------------------------------------------------------------------
bool CDXUTButton::HandleKeyboard( UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    if( !m_bEnabled || !m_bVisible )
        return false;

    switch( uMsg )
    {
        case WM_KEYDOWN:
        {
            switch( wParam )
            {
                case VK_SPACE:
                    m_bPressed = true;
                    return true;
            }
        }

        case WM_KEYUP:
        {
            switch( wParam )
            {
                case VK_SPACE:
                    if( m_bPressed == true )
                    {
                        m_bPressed = false;
                        m_pDialog->SendEvent( EVENT_BUTTON_CLICKED, true, this );
                    }
                    return true;
            }
        }
    }
    return false;
}


//--------------------------------------------------------------------------------------
bool CDXUTButton::HandleMouse( UINT uMsg, POINT pt, WPARAM wParam, LPARAM lParam )
{
    if( !m_bEnabled || !m_bVisible )
        return false;

    switch( uMsg )
    {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            {
                if( ContainsPoint( pt ) )
                {
                    // Pressed while inside the control
                    m_bPressed = true;
                    SetCapture( DXUTGetHWND() );

                    if( !m_bHasFocus )
                        m_pDialog->RequestFocus( this );

                    return true;
                }

                break;
            }

        case WM_LBUTTONUP:
        {
            if( m_bPressed )
            {
                m_bPressed = false;
                ReleaseCapture();

                if( !m_pDialog->m_bKeyboardInput )
                    m_pDialog->ClearFocus();

                // Button click
                if( ContainsPoint( pt ) )
                    m_pDialog->SendEvent( EVENT_BUTTON_CLICKED, true, this );

                return true;
            }

            break;
        }
    };

    return false;
}

//--------------------------------------------------------------------------------------
void CDXUTButton::Render( float fElapsedTime )
{
    if( m_bVisible == false )
        return;

    // Background fill layer
    CDXUTElement* pElement = m_Elements.GetAt( 0 );

    RECT rcWindow = m_rcBoundingBox;

    // Blend current color
	pElement->TextureColor.Current.a = 1.0f;
	pElement->TextureColor.Current.r = 1.0f;
	pElement->TextureColor.Current.g = 1.0f;
	pElement->TextureColor.Current.b = 1.0f;

	pElement->rcTexture.right = 328;
	pElement->rcTexture.bottom = 175;


    m_pDialog->DrawSprite( pElement, &rcWindow, DXUT_FAR_BUTTON_DEPTH );
}

//--------------------------------------------------------------------------------------
// CDXUTEditBox class
//--------------------------------------------------------------------------------------

// When scrolling, EDITBOX_SCROLLEXTENT is reciprocal of the amount to scroll.
// If EDITBOX_SCROLLEXTENT = 4, then we scroll 1/4 of the control each time.
#define EDITBOX_SCROLLEXTENT 4

#define IN_FLOAT_CHARSET( c ) \
    ( (c) == L'-' || (c) == L'.' || ( (c) >= L'0' && (c) <= L'9' ) )

//--------------------------------------------------------------------------------------
void DXUTBlendColor::Init( D3DCOLOR defaultColor, D3DCOLOR disabledColor, D3DCOLOR hiddenColor )
{
    for( int i = 0; i < MAX_CONTROL_STATES; i++ )
    {
        States[ i ] = defaultColor;
    }

    States[ DXUT_STATE_DISABLED ] = disabledColor;
    States[ DXUT_STATE_HIDDEN ] = hiddenColor;
    Current = hiddenColor;
}


//--------------------------------------------------------------------------------------
void DXUTBlendColor::Blend( UINT iState, float fElapsedTime, float fRate )
{
    D3DXCOLOR destColor = States[ iState ];
    D3DXColorLerp( &Current, &Current, &destColor, 1.0f - powf( fRate, 30 * fElapsedTime ) );
}



//--------------------------------------------------------------------------------------
void CDXUTElement::SetTexture( UINT iTexture, RECT* prcTexture, D3DCOLOR defaultTextureColor )
{
    this->iTexture = iTexture;

    if( prcTexture )
        rcTexture = *prcTexture;
    else
        SetRectEmpty( &rcTexture );

    TextureColor.Init( defaultTextureColor );
}

//--------------------------------------------------------------------------------------
void CDXUTElement::Refresh()
{
    TextureColor.Current = TextureColor.States[ DXUT_STATE_HIDDEN ];
}


