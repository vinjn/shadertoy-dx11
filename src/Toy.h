#include <windows.h>
#include <windowsx.h>
#include <xnamath.h>
#include <atlbase.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include "V.h"

const char kAppName[] = "HlslShaderToy";
const char kErrorBoxName[] = "Shader Compiling Error";
const int kAppWidth = 800;
const int kAppHeight = 600;
const int kFileChangeDetectionMS = 1000;
const float kBlackColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // red, green, blue, alpha

extern CComPtr<ID3D11Device>                    g_pd3dDevice;
extern CComPtr<ID3D11DeviceContext>             g_pContext;
extern  HWND g_hWnd;
extern CComPtr<ID3D11PixelShader>               g_pPixelShader;
extern std::vector<ID3D11ShaderResourceView*>   g_pTextureSRVs;      // Texture2D textures[];

extern FILETIME                                 g_lastModifyTime;
extern bool                                     g_failToCompileShader;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT SetupWindow( HINSTANCE hInstance, int nCmdShow );
void    DestroyWindow();
HRESULT SetupDevice();
void    DestroyDevice();
LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );
void Render();

HRESULT updateShaderAndTexturesFromFile(const std::string& filename);