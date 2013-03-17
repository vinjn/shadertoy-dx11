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
const char kErrorBoxName[] = "Shader Compiling Error";
const int kAppWidth = 800;
const int kAppHeight = 600;
const int kFileChangeDetectionMS = 1000;
const float kBlackColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // red, green, blue, alpha

using std::vector;
using std::string;

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

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------

struct CBOneFrame
{
    XMFLOAT2    resolution;     // viewport resolution (in pixels)
    float       time;     // shader playback time (in seconds)
    float       pad;             // padding
    XMFLOAT4    mouse;          // mouse pixel coords. xy: current (if MLB down), zw: click
    XMFLOAT4    date;           // (year, month, day, time in seconds)
}g_cbOneFrame;

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE                               g_hInst;
HWND                                    g_hWnd;
D3D_DRIVER_TYPE                         g_driverType;
D3D_FEATURE_LEVEL                       g_featureLevel;
CComPtr<ID3D11Device>                   g_pd3dDevice;
CComPtr<ID3D11DeviceContext>            g_pContext;
CComPtr<IDXGISwapChain>                 g_pSwapChain;

namespace PingPong
{
    CComPtr<ID3D11Texture2D>                Texs[2];
    CComPtr<ID3D11ShaderResourceView>       SRVs[2];
    CComPtr<ID3D11RenderTargetView>         RTVs[2];
}
int frontBufferIdx = 0;
int backBufferIdx = 1;

CComPtr<ID3D11RenderTargetView>         g_pRenderTargetView;
CComPtr<ID3D11Texture2D>                g_pBackbuffer;;

CComPtr<ID3D11VertexShader>             g_pVertexShader;
CComPtr<ID3D11PixelShader>              g_pPixelShader;
CComPtr<ID3D11Buffer>                   g_pCBOneFrame;      // cbuffer CBOneFrame;
std::vector<ID3D11ShaderResourceView*>  g_pTextureSRVs;      // Texture2D textures[];
CComPtr<ID3D11SamplerState>             g_pSamplerSmooth;
CComPtr<ID3D11SamplerState>             g_pSamplerBlocky;

// Setup the viewport
D3D11_VIEWPORT g_viewport;
std::vector<std::string>                g_texturePaths;
std::string                             g_pixelShaderFileName;
FILETIME                                g_lastModifyTime;
bool                                    g_failToCompileShader = false;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow );
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );
void Render();

std::string getOpenFilePath( const std::string &initialPath, std::vector<std::string> extensions );
std::string getAppPath();

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
        vector<string> extensions;
        extensions.push_back("hlsl");
        extensions.push_back("fx");
        g_pixelShaderFileName = getOpenFilePath(getAppPath(), extensions);
    }
    else
    {
        g_pixelShaderFileName = lpCmdLine;
        if (g_pixelShaderFileName[0] == '"')
            g_pixelShaderFileName = g_pixelShaderFileName.substr(1, g_pixelShaderFileName.length()-2);
    }

    if (g_pixelShaderFileName.length() == 0)
    {
        MessageBox(NULL, "Usage: HlslShaderToy.exe /path/to/pixel_shader.hlsl", kAppName, MB_OK);
        return -1;
    }

    if( FAILED( InitWindow( hInstance, nCmdShow ) ) )
        return 0;

    ::SetTimer(g_hWnd, 0, kFileChangeDetectionMS, NULL);

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
            ::Sleep(10);
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
    wcex.hCursor = LoadCursor( NULL, IDC_CROSS );
    wcex.lpszClassName = kAppName;
    if( !RegisterClassEx( &wcex ) )
        return E_FAIL;

    // Create window
    g_hInst = hInstance;
    RECT rc = { 0, 0, kAppWidth, kAppHeight};
    AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
    g_hWnd = CreateWindow( kAppName, kAppName, 
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 
        rc.right - rc.left, rc.bottom - rc.top, 
        NULL, NULL, hInstance,
        NULL );
    if( !g_hWnd )
        return E_FAIL;

    ShowWindow( g_hWnd, nCmdShow );

    return S_OK;
}

FILETIME getFileModifyTime(const std::string& filename)
{
    FILETIME modTime = {0};
    HANDLE handle = ::CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (handle != INVALID_HANDLE_VALUE)
    {
        BOOL is = GetFileTime(handle, NULL, NULL, &modTime);
        (is);
        CloseHandle(handle);
    }
    return modTime;
}

HRESULT updateShaderAndTexturesFromFile(const std::string& filename)
{
    std::stringstream ss;
    ss << "Opening shader file: " << filename <<"\n";
    OutputDebugStringA(ss.str().c_str());

    g_lastModifyTime = getFileModifyTime(filename);

    std::ifstream ifs(filename.c_str());
    if (!ifs)
    {
        return D3D11_ERROR_FILE_NOT_FOUND;
    }

    g_texturePaths.clear();
    const std::regex reComment(".*//\\s*(.*)");
    // e:\\__svn_pool\\HlslShaderToy\\media\\ducky.png

    std::stringstream pixelShaderText;
    std::string oneline;
    while (std::getline(ifs, oneline))
    {
        pixelShaderText << oneline << "\n";

        std::smatch sm;
        if (std::regex_match (oneline, sm, reComment))
        {
            std::string possiblePath = sm.str(1);
            D3DX11_IMAGE_INFO imageInfo;

            if (std::ifstream(possiblePath.c_str()))
            {
                if (SUCCEEDED(D3DX11GetImageInfoFromFile(possiblePath.c_str(), NULL, &imageInfo, NULL)))
                {
                    g_texturePaths.push_back(possiblePath);
                    std::stringstream ss;
                    ss << "Loading image from: " << possiblePath <<"\n";
                    OutputDebugStringA(ss.str().c_str());
                }
            }

        }
    }
    ifs.close();

    std::stringstream pixelShaderHeaderSS;
    if (!g_texturePaths.empty())
    {
        pixelShaderHeaderSS << "Texture2D textures[" << g_texturePaths.size() <<"] : register( t0 );\n";
    }

    pixelShaderHeaderSS <<
        "Texture2D backbuffer : register( t1 );\n"
        "\n"
        "SamplerState smooth : register( s0 );\n"
        "SamplerState blocky : register( s1 );\n"
        "\n"
        "cbuffer CBOneFrame : register( b0 )\n"
        "{\n"
        "    float2     resolution;     // viewport resolution (in pixels)\n"
        "    float      time;           // shader playback time (in seconds)\n"
        "    float      pad;            // padding\n"
        "    float4     mouse;          // mouse pixel coords. xy: current (if MLB down), zw: click\n"
        "    float4     date;           // (year, month, day, time in seconds)\n"
        "};\n"
        "\n"
        ;

    std::string pixelShaderHeader = pixelShaderHeaderSS.str();
    const size_t nCommonShaderNewLines = std::count(pixelShaderHeader.begin(), pixelShaderHeader.end(), '\n');

    // add together
    std::string psText = pixelShaderHeader + pixelShaderText.str();

    // output complete shader file
    {
        std::ofstream completeShaderFile((filename+".expanded.txt").c_str());
        if (completeShaderFile)
        {
            completeShaderFile << psText;
        }
    }

    ID3DBlob* pPSBlob = NULL;
    std::string errorMsg;
    hr = CompileShaderFromMemory( psText.c_str(), "main", "ps_4_0", &pPSBlob, &errorMsg );

    // hack shader compiling error message
    if (FAILED(hr))
    {
        size_t posCarriageReturn = 0;   // 10
        size_t posComma = 0;            // ','

        while (true)
        {
            posComma = errorMsg.find(',', posCarriageReturn);
            if (posComma == std::string::npos)
                break;

            size_t length = posComma - posCarriageReturn - 1;
            std::string lineStr = errorMsg.substr(posCarriageReturn+1, length);
            int lineNo; 
            std::istringstream( lineStr ) >> lineNo;
            lineNo -= nCommonShaderNewLines;
            std::ostringstream ss;
            ss << lineNo;
            errorMsg.replace(posCarriageReturn+1, length, ss.str());

            posCarriageReturn = errorMsg.find(10, posComma);     
            if (posCarriageReturn == std::string::npos)
                break;
            posCarriageReturn ++;
        }

        //::Beep( rand()%200+700, 300 );
        g_failToCompileShader = true;
        OutputDebugStringA(errorMsg.c_str());
        ::MessageBox(g_hWnd, errorMsg.c_str(), kErrorBoxName, MB_OK);

        return E_FAIL;
    }

    g_failToCompileShader = false;
    g_pPixelShader = NULL;
    V_RETURN(g_pd3dDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShader ));

    std::stringstream titleSS;
    titleSS << filename << " - " << kAppName;
    ::SetWindowText(g_hWnd, titleSS.str().c_str());

#ifdef TEST_SHADER_REFLECTION
    // shader reflection
    {
        ID3D11ShaderReflection* pReflector = NULL; 
        V_RETURN( D3DReflect( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), 
            IID_ID3D11ShaderReflection, (void**) &pReflector) );

        D3D11_SHADER_DESC desc;
        V_RETURN(pReflector->GetDesc(&desc));

        UINT nCBs = desc.ConstantBuffers;
        UNREFERENCED_PARAMETER(nCBs);
    }
#endif

    for (size_t i=0;i<g_pTextureSRVs.size();i++)
    {
        SAFE_RELEASE(g_pTextureSRVs[i]);
    }

    for (int i=0;i<2;i++)
    {
        g_pContext->ClearRenderTargetView( PingPong::RTVs[i], kBlackColor );
    }

    g_pTextureSRVs.resize(g_texturePaths.size());
    for (size_t i=0;i<g_texturePaths.size();i++)
    {
        V_RETURN(D3DX11CreateShaderResourceViewFromFile( g_pd3dDevice, g_texturePaths[i].c_str(), NULL, NULL, &g_pTextureSRVs[i], NULL ));
    }

    ::Beep( rand()%100+200, 300 );

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

    g_viewport = CD3D11_VIEWPORT(0.0f, 0.0f, (float)width, (float)height);

    g_cbOneFrame.resolution.x = (float)width;
    g_cbOneFrame.resolution.y = (float)height;

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
            D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pContext );
        if( SUCCEEDED( hr ) )
            break;
    }
    if( FAILED( hr ) )
        return hr;

    V_RETURN(g_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( LPVOID* )&g_pBackbuffer ));
    V_RETURN(g_pd3dDevice->CreateRenderTargetView( g_pBackbuffer , NULL, &g_pRenderTargetView ));

    CD3D11_TEXTURE2D_DESC texDesc(sd.BufferDesc.Format, 
        sd.BufferDesc.Width, sd.BufferDesc.Height,
        1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

    // create ping-pong RTV & SRV
    for (int i=0;i<2;i++)
    {
        V_RETURN(g_pd3dDevice->CreateTexture2D( &texDesc, NULL, &PingPong::Texs[i] ));
        V_RETURN(g_pd3dDevice->CreateRenderTargetView( PingPong::Texs[i], NULL, &PingPong::RTVs[i] ));
        ((ID3D11Texture2D*)PingPong::Texs[i])->Release();
        V_RETURN(g_pd3dDevice->CreateShaderResourceView( PingPong::Texs[i], NULL, &PingPong::SRVs[i] ));
        ((ID3D11Texture2D*)PingPong::Texs[i])->Release();
    }

    {
        ID3DBlob* pVSBlob = NULL;
        V_RETURN(CompileShaderFromMemory( kVertexShaderCode.c_str(), "VS", "vs_4_0", &pVSBlob ));
        V_RETURN(g_pd3dDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVertexShader ));
    }

    {
        CD3D11_BUFFER_DESC desc(sizeof(CBOneFrame), D3D11_BIND_CONSTANT_BUFFER);
        V_RETURN(g_pd3dDevice->CreateBuffer( &desc, NULL, &g_pCBOneFrame ));
    }

    V(updateShaderAndTexturesFromFile(g_pixelShaderFileName));

    // Create the sample state
    CD3D11_SAMPLER_DESC sampDesc(D3D11_DEFAULT);
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN(g_pd3dDevice->CreateSamplerState( &sampDesc, &g_pSamplerSmooth ));

    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    V_RETURN(g_pd3dDevice->CreateSamplerState( &sampDesc, &g_pSamplerBlocky ));

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
    if( g_pContext ) g_pContext->ClearState();

    for (size_t i=0;i<g_pTextureSRVs.size();i++)
    {
        SAFE_RELEASE(g_pTextureSRVs[i]);
    }
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
    case WM_TIMER:
        {
            FILETIME ftime = getFileModifyTime(g_pixelShaderFileName);
            if (ftime.dwLowDateTime != g_lastModifyTime.dwLowDateTime || ftime.dwHighDateTime != g_lastModifyTime.dwHighDateTime)
            {
                ftime = g_lastModifyTime;
                if (g_failToCompileShader)
                {
                    // close the previous MessageBox
                    if (HWND hBox = ::FindWindow(NULL, kErrorBoxName))
                    {
                        ::SendMessage(hBox, WM_CLOSE, 0, 0);
                    }
                }
                updateShaderAndTexturesFromFile(g_pixelShaderFileName);
            }
        }break;
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
            g_cbOneFrame.mouse.x = (float)mouseX;
            g_cbOneFrame.mouse.y = (float)mouseY;
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
    g_cbOneFrame.time = ( GetTickCount() - dwTimeStart ) / 1000.0f;

    g_pContext->ClearRenderTargetView( PingPong::RTVs[frontBufferIdx], kBlackColor );

    ID3D11RenderTargetView* pRTVs[] = {PingPong::RTVs[frontBufferIdx]};
    g_pContext->OMSetRenderTargets( 1, pRTVs, NULL );
    g_pContext->RSSetViewports( 1, &g_viewport );

    g_pContext->UpdateSubresource( g_pCBOneFrame, 0, NULL, &g_cbOneFrame, 0, 0 );

    g_pContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    g_pContext->VSSetShader( g_pVertexShader, NULL, 0 );

    g_pContext->PSSetShader( g_pPixelShader, NULL, 0 );
    ID3D11Buffer* pCBuffers[] = {g_pCBOneFrame};
    g_pContext->PSSetConstantBuffers( 0, 1, pCBuffers );

    if (!g_pTextureSRVs.empty())
        g_pContext->PSSetShaderResources( 0, g_pTextureSRVs.size(), &g_pTextureSRVs[0] );
    ID3D11ShaderResourceView* pSRVs[] = {PingPong::SRVs[backBufferIdx]};
    g_pContext->PSSetShaderResources( 1, 1, pSRVs );

    ID3D11SamplerState* pSamplers[] = {g_pSamplerSmooth, g_pSamplerBlocky};
    g_pContext->PSSetSamplers( 0, 2, pSamplers );

    g_pContext->Draw( 3, 0 );

    g_pContext->CopyResource(g_pBackbuffer, PingPong::Texs[frontBufferIdx]);

    g_pSwapChain->Present( 0, 0 );

    std::swap(frontBufferIdx, backBufferIdx);

    g_pContext->ClearState();
}

std::string getOpenFilePath( const std::string &initialPath, std::vector<std::string> extensions )
{
    OPENFILENAMEA ofn;       // common dialog box structure
    char szFile[260];       // buffer for file name

    // Initialize OPENFILENAME
    ::ZeroMemory( &ofn, sizeof(ofn) );
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFile = szFile;

    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not
    // use the contents of szFile to initialize itself.
    //
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof( szFile );
    if( extensions.empty() ) {
        ofn.lpstrFilter = "All\0*.*\0";
    }
    else {
        char extensionStr[10000];
        size_t offset = 0;

        strcpy( extensionStr, "Supported Types" );
        offset += strlen( extensionStr ) + 1;
        for( vector<string>::const_iterator strIt = extensions.begin(); strIt != extensions.end(); ++strIt ) {
            strcpy( extensionStr + offset, "*." );
            offset += 2;
            strcpy( extensionStr + offset, strIt->c_str() );
            offset += strIt->length();
            // append a semicolon to all but the last extensions
            if( strIt + 1 != extensions.end() ) {
                extensionStr[offset] = ';';
                offset += 1;
            }
            else {
                extensionStr[offset] = 0;
                offset += 1;
            }
        }

        extensionStr[offset] = 0;
        ofn.lpstrFilter = extensionStr;
    }
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    if( initialPath.empty() )
        ofn.lpstrInitialDir = NULL;
    else {
        char initialPathStr[MAX_PATH];
        strcpy( initialPathStr, initialPath.c_str() );
        ofn.lpstrInitialDir = initialPathStr;
    }
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    // Display the Open dialog box.
    if( ::GetOpenFileNameA( &ofn ) == TRUE ) {
        return string( ofn.lpstrFile );
    }
    else
        return string();
}

std::string getAppPath()
{
    char appPath[MAX_PATH] = "";

    // fetch the path of the executable
    ::GetModuleFileNameA( 0, appPath, sizeof(appPath) - 1);

    // get a pointer to the last occurrence of the windows path separator
    char *appDir = strrchr( appPath, '\\' );
    if( appDir ) {
        ++appDir;

        // always expect the unexpected - this shouldn't be null but one never knows
        if( appDir ) {
            // null terminate the string
            *appDir = 0;
        }
    }

    return std::string( appPath );
}