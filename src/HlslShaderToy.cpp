#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>
#include <atlbase.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <algorithm>
#include "V.h"

HRESULT hr = S_OK;

const char kAppName[] = "HlslShaderToy";
const int kAppWidth = 800;
const int kAppHeight = 600;

namespace std
{
    using namespace tr1;
}

const std::string kVertexShaderCode =

"float4 VS(uint id : SV_VertexID) : SV_POSITION\n"
"{\n"
"    float2 tex = float2((id << 1) & 2, id & 2);\n"
"    return float4(tex * float2(2,-2) + float2(-1,1), 0, 1);\n"
"}\n"
;

const std::string kPixelShaderHeader = 
"Texture2D iChannel[4] : register( t0 );\n"

"SamplerState samLinear : register( s0 );\n"

"cbuffer cbNeverChanges : register( b0 )\n"
"{\n"
"    float2      iResolution;     // viewport resolution (in pixels)\n"
"    float       iGlobalTime;     // shader playback time (in seconds)\n"
"    float       pad;             // padding\n"
"    float       iChannelTime[4]; // channel playback time (in seconds)\n"
"    float4      iMouse;          // mouse pixel coords. xy: current (if MLB down), zw: click\n"
"    float4      iDate;           // (year, month, day, time in seconds)\n"
"};\n"

"struct PS_Input\n"
"{\n"
"    float4 pos : SV_POSITION;\n"
"    float2 tex : TEXCOORD0;\n"
"};\n"

"typedef float2 vec2;\n"
"typedef float3 vec3;\n"
"typedef float4 vec4;\n"
"typedef int2 ivec2;\n"
"typedef int3 ivec3;\n"
"typedef int4 ivec4;\n"
"typedef float2x2 mat2;\n"
"typedef float3x3 mat3;\n"
"typedef float4x4 mat4;\n"
;

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------

struct CBOneFrame
{
    XMFLOAT2    iResolution;     // viewport resolution (in pixels)
    float       iGlobalTime;     // shader playback time (in seconds)
    float       pad;             // padding
    float       iChannelTime[4]; // channel playback time (in seconds)
    XMFLOAT4    iMouse;          // mouse pixel coords. xy: current (if MLB down), zw: click
    XMFLOAT4    iDate;           // (year, month, day, time in seconds)
}g_cbOneFrame;

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE                               g_hInst;
HWND                                    g_hWnd;
D3D_DRIVER_TYPE                         g_driverType;
D3D_FEATURE_LEVEL                       g_featureLevel;
CComPtr<ID3D11Device>                   g_pd3dDevice;
CComPtr<ID3D11DeviceContext>            g_pImmediateContext;
CComPtr<IDXGISwapChain>                 g_pSwapChain;
CComPtr<ID3D11RenderTargetView>         g_pRenderTargetView;
CComPtr<ID3D11VertexShader>             g_pVertexShader;
CComPtr<ID3D11PixelShader>              g_pPixelShader;
CComPtr<ID3D11Buffer>                   g_pCBOneFrame;
std::vector<ID3D11ShaderResourceView*>       g_pTextureRVs;
CComPtr<ID3D11SamplerState>             g_pSamplerLinear;

std::vector<std::string>                g_texturePaths;
std::string                             g_pixelShaderFileName;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow );
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );
void Render();

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
    UNREFERENCED_PARAMETER( hPrevInstance );
    UNREFERENCED_PARAMETER( lpCmdLine );

    if (strlen(lpCmdLine) == 0)
    {
        MessageBox(NULL, "Usage: HlslShaderToy.exe /path/to/pixel_shader.hlsl", kAppName, MB_OK);
        return -1;
    }
    else
    {
        g_pixelShaderFileName = lpCmdLine;
        if (g_pixelShaderFileName[0] == '"')
            g_pixelShaderFileName = g_pixelShaderFileName.substr(1, g_pixelShaderFileName.length()-2);
    }

    if( FAILED( InitWindow( hInstance, nCmdShow ) ) )
        return 0;

    if( FAILED( InitDevice() ) )
    {
        CleanupDevice();
        return 0;
    }

    // Main message loop
    MSG msg = {0};
    while( WM_QUIT != msg.message )
    {
        if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
        else
        {
            Render();
        }
    }

    CleanupDevice();

    return ( int )msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow )
{
    // Register class
    WNDCLASSEX wcex = {sizeof( WNDCLASSEX )};
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor( NULL, IDC_ARROW );
    wcex.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
    wcex.lpszClassName = kAppName;
    if( !RegisterClassEx( &wcex ) )
        return E_FAIL;

    // Create window
    g_hInst = hInstance;
    RECT rc = { 0, 0, kAppWidth, kAppHeight};
    AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
    g_hWnd = CreateWindow( kAppName, kAppName, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
        NULL );
    if( !g_hWnd )
        return E_FAIL;

    ShowWindow( g_hWnd, nCmdShow );

    return S_OK;
}

HRESULT CreatePixelShaderFromFile(LPCSTR filename)
{
    // open
    const size_t nCommonShaderNewLines = std::count(kPixelShaderHeader.begin(), kPixelShaderHeader.end(), '\n');
    std::ifstream ifs(filename, std::ifstream::binary);
    if (!ifs)
    {
        return E_FAIL;
    }

    g_texturePaths.clear();
    std::regex re(".*//\\s*(.*)");
    // e:\\__svn_pool\\HlslShaderToy\\media\\ducky.png

    std::stringstream pixelShaderText;
    std::string oneline;
    while (std::getline(ifs, oneline))
    {
        pixelShaderText << oneline;

        std::string smaller = oneline.substr(0, oneline.length() - 1);
        std::smatch sm;
        std::regex_match (smaller, sm, re);
        if (!sm.empty())
        {
            std::string possiblePath = sm.str(1);
            D3DX11_IMAGE_INFO imageInfo;

            if (SUCCEEDED(D3DX11GetImageInfoFromFile(possiblePath.c_str(), NULL, &imageInfo, NULL)))
            {
                g_texturePaths.push_back(possiblePath);
                OutputDebugStringA(possiblePath.c_str());
                OutputDebugStringA("\n");
            }
        }
    }

    // add together
    std::string psText = kPixelShaderHeader + pixelShaderText.str();

    ID3DBlob* pPSBlob = NULL;
    std::string errorMsg;
    hr = CompileShaderFromMemory( psText.c_str(), "main", "ps_4_0", &pPSBlob, &errorMsg );

    // hack shader compiling error message
    if (FAILED(hr))
    {
        size_t lineStrSize = errorMsg.find(',') - 1;
        std::string lineStr = errorMsg.substr(1, lineStrSize);
        int lineNo; 
        std::istringstream( lineStr ) >> lineNo;
        lineNo -= nCommonShaderNewLines;
        std::ostringstream ss;
        ss << lineNo;
        errorMsg.replace(1, lineStrSize, ss.str());
        OutputDebugStringA(errorMsg.c_str());
        
        return E_FAIL;
    }

    g_pPixelShader = NULL;
    V_RETURN(g_pd3dDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShader ));

    // shader reflection
    {
        ID3D11ShaderReflection* pReflector = NULL; 
        V_RETURN( D3DReflect( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), 
            IID_ID3D11ShaderReflection, (void**) &pReflector) );

        D3D11_SHADER_DESC desc;
        V_RETURN(pReflector->GetDesc(&desc));

        UINT nCBs = desc.ConstantBuffers;
    }
    return hr;
}

//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
    RECT rc;
    GetClientRect( g_hWnd, &rc );
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    g_cbOneFrame.iResolution.x = (float)width;
    g_cbOneFrame.iResolution.y = (float)height;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE( driverTypes );

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT numFeatureLevels = ARRAYSIZE( featureLevels );

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory( &sd, sizeof( sd ) );
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    for( UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++ )
    {
        g_driverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDeviceAndSwapChain( NULL, g_driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels,
            D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext );
        if( SUCCEEDED( hr ) )
            break;
    }
    if( FAILED( hr ) )
        return hr;

    // Create a render target view
    ID3D11Texture2D* pBackBuffer = NULL;
    V_RETURN(g_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( LPVOID* )&pBackBuffer ));

    hr = g_pd3dDevice->CreateRenderTargetView( pBackBuffer, NULL, &g_pRenderTargetView );
    pBackBuffer->Release();
    if( FAILED( hr ) )
        return hr;

    ID3D11RenderTargetView* pRTVs[] = {g_pRenderTargetView};
    g_pImmediateContext->OMSetRenderTargets( 1, pRTVs, NULL );

    // Setup the viewport
    CD3D11_VIEWPORT vp(0.0f, 0.0f, (float)width, (float)height);
    g_pImmediateContext->RSSetViewports( 1, &vp );

    {
        ID3DBlob* pVSBlob = NULL;
        V_RETURN(CompileShaderFromMemory( kVertexShaderCode.c_str(), "VS", "vs_4_0", &pVSBlob ));
        V_RETURN(g_pd3dDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVertexShader ));
    }

    V_RETURN(CreatePixelShaderFromFile(g_pixelShaderFileName.c_str()));

    {
        CD3D11_BUFFER_DESC desc(sizeof(CBOneFrame), D3D11_BIND_CONSTANT_BUFFER);
        V_RETURN(g_pd3dDevice->CreateBuffer( &desc, NULL, &g_pCBOneFrame ));
    }

    // Load the Texture
    // TODO: release
    g_pTextureRVs.clear();
    g_pTextureRVs.resize(g_texturePaths.size());
    for (size_t i=0;i<g_texturePaths.size();i++)
    {
        V_RETURN(D3DX11CreateShaderResourceViewFromFile( g_pd3dDevice, g_texturePaths[i].c_str(), NULL, NULL, &g_pTextureRVs[i], NULL ));
    }

    // Create the sample state
    CD3D11_SAMPLER_DESC sampDesc(D3D11_DEFAULT);
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN(g_pd3dDevice->CreateSamplerState( &sampDesc, &g_pSamplerLinear ));

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
    if( g_pImmediateContext ) g_pImmediateContext->ClearState();
}


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    PAINTSTRUCT ps;
    HDC hdc;

    int mouseX = GET_X_LPARAM(lParam);
    int mouseY = GET_Y_LPARAM(lParam);

    switch( message )
    {
    case WM_PAINT:
        {
            hdc = BeginPaint( hWnd, &ps );
            EndPaint( hWnd, &ps );
        }break;
    case WM_KEYUP:
        {
            switch (wParam)
            {
            case VK_ESCAPE:
                {
                    ::PostQuitMessage(0);
                }break;
            }

        }break;
    case WM_MOUSEMOVE:
        {
            g_cbOneFrame.iMouse.x = (float)mouseX;
            g_cbOneFrame.iMouse.y = (float)mouseY;
        }break;
    case WM_DESTROY:
        PostQuitMessage( 0 );
        break;
    default:
        return DefWindowProc( hWnd, message, wParam, lParam );
    }

    return 0;
}


//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
    static DWORD dwTimeStart = GetTickCount();
    g_cbOneFrame.iGlobalTime = ( GetTickCount() - dwTimeStart ) / 1000.0f;

    {
        float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // red, green, blue, alpha
        g_pImmediateContext->ClearRenderTargetView( g_pRenderTargetView, ClearColor );
    }

    g_pImmediateContext->UpdateSubresource( g_pCBOneFrame, 0, NULL, &g_cbOneFrame, 0, 0 );

    g_pImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    g_pImmediateContext->VSSetShader( g_pVertexShader, NULL, 0 );

    g_pImmediateContext->PSSetShader( g_pPixelShader, NULL, 0 );
    ID3D11Buffer* pCBuffers[] = {g_pCBOneFrame};
    g_pImmediateContext->PSSetConstantBuffers( 0, 1, pCBuffers );
    g_pImmediateContext->PSSetShaderResources( 0, std::min<int>(4, g_pTextureRVs.size()), &g_pTextureRVs[0] );
    ID3D11SamplerState* pSamplers[] = {g_pSamplerLinear};
    g_pImmediateContext->PSSetSamplers( 0, 1, pSamplers );

    g_pImmediateContext->Draw( 3, 0 );

    g_pSwapChain->Present( 0, 0 );
}
