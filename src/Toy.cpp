#include "Toy.h"
#include "resource.h"

HRESULT hr = S_OK;

using std::vector;
using std::string;

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------

struct CBOneFrame
{
    XMFLOAT2    resolution;     // viewport resolution (in pixels)
    float       time;           // shader playback time (in seconds)
    float       aspect;         // cached aspect of viewport
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
CComPtr<ID3D11SamplerState>             g_pSamplerMirror;

// Setup the viewport
D3D11_VIEWPORT g_viewport;
std::string                             g_pixelShaderFileName;
FILETIME                                g_lastModifyTime;
bool                                    g_failToCompileShader = false;

const std::string kVertexShaderCode =
"float4 VS(uint id : SV_VertexID) : SV_POSITION\n"
"{\n"
"    float2 tex = float2((id << 1) & 2, id & 2);\n"
"    return float4(tex * float2(2,-2) + float2(-1,1), 0, 1);\n"
"}\n"
;

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
        extensions.push_back("toy");
        g_pixelShaderFileName = getOpenFilePath(g_hWnd, getAppPath(), extensions);
    }
    else
    {
        g_pixelShaderFileName = lpCmdLine;
        if (g_pixelShaderFileName[0] == '"')
            g_pixelShaderFileName = g_pixelShaderFileName.substr(1, g_pixelShaderFileName.length()-2);
    }

    if (g_pixelShaderFileName.length() == 0)
    {
        MessageBox(NULL, "Usage: HlslShaderToy.exe /path/to/pixel_shader.toy", kAppName, MB_OK);
        return -1;
    }

    if( FAILED( SetupWindow( hInstance, nCmdShow ) ) )
        return 0;

    ::SetTimer(g_hWnd, 0, kFileChangeDetectionMS, NULL);

    if( FAILED( SetupDevice() ) )
    {
        DestroyDevice();
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

    DestroyDevice();

    return ( int )msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT SetupWindow( HINSTANCE hInstance, int nCmdShow )
{
    // Register class
    WNDCLASSEX wcex = {sizeof( WNDCLASSEX )};
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon  =   ::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
    wcex.hCursor = LoadCursor( NULL, IDC_CROSS );
    wcex.lpszClassName = kAppName;
    if( !::RegisterClassEx( &wcex ) )
        return E_FAIL;

    // Create window
    g_hInst = hInstance;
    RECT rc = { 0, 0, kAppWidth, kAppHeight};
    ::AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
    g_hWnd = CreateWindow( kAppName, kAppName, 
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 
        rc.right - rc.left, rc.bottom - rc.top, 
        NULL, NULL, hInstance,
        NULL );
    if( !g_hWnd )
        return E_FAIL;

    ::ShowWindow( g_hWnd, nCmdShow );

    ::DragAcceptFiles( g_hWnd, TRUE );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT SetupDevice()
{
    RECT rc;
    GetClientRect( g_hWnd, &rc );
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    g_viewport = CD3D11_VIEWPORT(0.0f, 0.0f, (float)width, (float)height);

    g_cbOneFrame.resolution.x = (float)width;
    g_cbOneFrame.resolution.y = (float)height;
    g_cbOneFrame.aspect = (float)width / (float)height;

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

    CD3D11_SAMPLER_DESC sampDesc(D3D11_DEFAULT);
    sampDesc.AddressU = sampDesc.AddressV = sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    V_RETURN(g_pd3dDevice->CreateSamplerState( &sampDesc, &g_pSamplerSmooth ));

    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    V_RETURN(g_pd3dDevice->CreateSamplerState( &sampDesc, &g_pSamplerBlocky ));

    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = sampDesc.AddressV = sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
    V_RETURN(g_pd3dDevice->CreateSamplerState( &sampDesc, &g_pSamplerMirror ));

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void DestroyDevice()
{
    if( g_pContext ) g_pContext->ClearState();

    for (size_t i=0;i<g_pTextureSRVs.size();i++)
    {
        SAFE_RELEASE(g_pTextureSRVs[i]);
    }
}


void DestroyWindow()
{
    if( g_hWnd )
        ::DestroyWindow( g_hWnd );
    UnregisterClass(kAppName, g_hInst);
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

    bool isShaderDirty = false;

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
                isShaderDirty = true;
            }
        }break;
    case WM_DROPFILES: 
        {
            HDROP dropH = (HDROP)wParam;
            char fileName[8192];

            int droppedFileCount = ::DragQueryFile( dropH, 0xFFFFFFFF, 0, 0 );
            for( int i = 0; i < droppedFileCount; ++i ) 
            {
                ::DragQueryFileA( dropH, i, fileName, 8192 );
                if (strstr(fileName, ".toy"))
                {
                    g_pixelShaderFileName = fileName;
                    isShaderDirty = true;
                    break;
                }
            }

            if (!isShaderDirty)
            {
                ::MessageBox(g_hWnd, "Please drag a shader file (.toy).", kAppName, MB_OK);
            }

            ::DragFinish( dropH );
        }break;
    case WM_PAINT:
        {
            hdc = ::BeginPaint( hWnd, &ps );
            ::EndPaint( hWnd, &ps );
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

    if (isShaderDirty)
    {
        updateShaderAndTexturesFromFile(g_pixelShaderFileName);
        for (int i=0;i<2;i++)
        {
            g_pContext->ClearRenderTargetView( PingPong::RTVs[i], kBlackColor );
        }
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
