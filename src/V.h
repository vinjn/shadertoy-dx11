#pragma once

#include <d3d11.h>
#include <d3dx11.h>
#include <dxerr.h>
#include <string>
#include <vector>

#if defined(DEBUG) || defined(_DEBUG)
#ifndef V
#define V(x)           { hr = (x); if( FAILED(hr) ) { DXTrace( __FILE__, (DWORD)__LINE__, hr, #x, false ); } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return DXTrace( __FILE__, (DWORD)__LINE__, hr, #x, false ); } }
#endif
#else
#ifndef V
#define V(x)           { hr = (x); }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return hr; } }
#endif
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)       { if (p) { delete (p);     (p)=NULL; } }
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p);   (p)=NULL; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif  


//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DX11
//--------------------------------------------------------------------------------------
HRESULT compileShaderFromFile( LPCSTR szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut );

HRESULT compileShaderFromMemory( LPCSTR szText, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, std::string* pErrorStr = NULL);

std::string getOpenFilePath( HWND hWnd, const std::string &initialPath, std::vector<std::string> extensions );
std::string getAppPath();
FILETIME getFileModifyTime(const std::string& filename);

HRESULT downloadFromUrl( const std::string& url, const std::string& localPath);
std::string getTempFolder();

// https://github.com/vinjn/HlslShaderToy/issues/10 on hard drive
std::string extractToyFromGithubIssuse( const std::string& localHtmlFile );
