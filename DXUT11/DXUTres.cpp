//----------------------------------------------------------------------------
// File: DXUTRes.cpp
//
// Copyright (c) Microsoft Corp. All rights reserved.
//-----------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTres.h"
#include <atlstr.h>

//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTCreateGUITextureFromInternalArray11(ID3D11Device* pd3dDevice, ID3D11Texture2D** ppTexture, D3DX11_IMAGE_INFO* pInfo)
{
    HRESULT hr;

    ID3D11Resource *pRes;
    D3DX11_IMAGE_LOAD_INFO loadInfo;
    loadInfo.Width = D3DX11_DEFAULT;
    loadInfo.Height  = D3DX11_DEFAULT;
    loadInfo.Depth  = D3DX11_DEFAULT;
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

	// Í¼Æ¬Â·¾¶
	CString path;
	::GetModuleFileName(NULL, path.GetBufferSetLength(MAX_PATH), MAX_PATH);
	path = path.Left(path.ReverseFind(L'\\')) + L"\\aa.png";
	//
	hr = D3DX11CreateTextureFromFile(pd3dDevice, path, &loadInfo, NULL, &pRes, NULL);
    if( FAILED( hr ) )
        return hr;
    hr = pRes->QueryInterface( __uuidof( ID3D11Texture2D ), (LPVOID*)ppTexture );
    SAFE_RELEASE( pRes );

    return S_OK;
}