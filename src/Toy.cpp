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
}gCbOneFrame;

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE                               gHInst;
HWND                                    gHWnd;
CComPtr<ID3D11Device>                   gDevice;
CComPtr<ID3D11DeviceContext>            gContext;
CComPtr<IDXGISwapChain>                 gSwapChain;

namespace PingPong
{
    CComPtr<ID3D11Texture2D>                TEXs[2];
    CComPtr<ID3D11ShaderResourceView>       SRVs[2];
    CComPtr<ID3D11RenderTargetView>         RTVs[2];
    int frontBufferIdx  = 0;
    int backBufferIdx   = 1;
}


CComPtr<ID3D11RenderTargetView>         gRenderTargetView;
CComPtr<ID3D11Texture2D>                gBackbuffer;;

CComPtr<ID3D11VertexShader>             gVertexShader;
CComPtr<ID3D11PixelShader>              gPixelShader;
CComPtr<ID3D11Buffer>                   gCBOneFrame;      // cbuffer CBOneFrame;
std::vector<ID3D11ShaderResourceView*>  gTextureSRVs;      // Texture2D textures[];
CComPtr<ID3D11SamplerState>             gSamplerSmooth;
CComPtr<ID3D11SamplerState>             gSamplerBlocky;
CComPtr<ID3D11SamplerState>             gSamplerMirror;

D3D11_VIEWPORT g_viewport;
std::string                             gToyFileName;
FILETIME                                gLastModifyTime;
bool                                    gFailsToCompileShader = false;
bool                                    gNeesToOutputCompleteHlsl = false;

bool                                    gIsCameraDevice = false;
videoInput                              gVideoInput;

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

    std::string toyFileName;

    if (strlen(lpCmdLine) == 0)
    {
        vector<string> extensions;
        extensions.push_back("toy");
        toyFileName = getOpenFilePath(gHWnd, getAppPath(), extensions);
    }
    else
    {
        toyFileName = lpCmdLine;
        if (toyFileName[0] == '"')
            toyFileName = toyFileName.substr(1, toyFileName.length()-2);
    }

    if (toyFileName.empty())
    {
        MessageBox(NULL, "Usage: HlslShaderToy.exe /path/to/pixel_shader.toy", kAppName, MB_OK);
        return -1;
    }

    if( FAILED( SetupWindow( hInstance, nCmdShow ) ) )
        return 0;

    ::SetTimer(gHWnd, 0, kFileChangeDetectionMS, NULL);

    if( FAILED( SetupDevice(toyFileName) ) )
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
    gHInst = hInstance;
    RECT rc = { 0, 0, kAppWidth, kAppHeight};
    ::AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
    gHWnd = CreateWindow( kAppName, kAppName, 
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 
        rc.right - rc.left, rc.bottom - rc.top, 
        NULL, NULL, hInstance,
        NULL );
    if( !gHWnd )
        return E_FAIL;

    ::ShowWindow( gHWnd, nCmdShow );

    ::DragAcceptFiles( gHWnd, TRUE );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT SetupDevice( const std::string& toyFullPath )
{
    RECT rc;
    ::GetClientRect( gHWnd, &rc );
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    g_viewport = CD3D11_VIEWPORT(0.0f, 0.0f, (float)width, (float)height);

    gCbOneFrame.resolution.x = (float)width;
    gCbOneFrame.resolution.y = (float)height;
    gCbOneFrame.aspect = (float)width / (float)height;

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
    sd.OutputWindow = gHWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    for( UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++ )
    {
        D3D_FEATURE_LEVEL   featureLevel;
        D3D_DRIVER_TYPE     driverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDeviceAndSwapChain( NULL, driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels,
            D3D11_SDK_VERSION, &sd, &gSwapChain, &gDevice, &featureLevel, &gContext );
        if (SUCCEEDED(hr))
            break;
    }
    if (FAILED(hr))
        return hr;

    V_RETURN(gSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( LPVOID* )&gBackbuffer ));
    V_RETURN(gDevice->CreateRenderTargetView( gBackbuffer , NULL, &gRenderTargetView ));

    CD3D11_TEXTURE2D_DESC texDesc(sd.BufferDesc.Format, 
        sd.BufferDesc.Width, sd.BufferDesc.Height,
        1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

    // create ping-pong RTV & SRV
    for (int i=0; i<2; i++)
    {
        V_RETURN(gDevice->CreateTexture2D( &texDesc, NULL, &PingPong::TEXs[i] ));
        V_RETURN(gDevice->CreateRenderTargetView( PingPong::TEXs[i], NULL, &PingPong::RTVs[i] ));
        ((ID3D11Texture2D*)PingPong::TEXs[i])->Release();
        V_RETURN(gDevice->CreateShaderResourceView( PingPong::TEXs[i], NULL, &PingPong::SRVs[i] ));
        ((ID3D11Texture2D*)PingPong::TEXs[i])->Release();
    }

    {
        ID3DBlob* pVSBlob = NULL;
        V_RETURN(compileShaderFromMemory( kVertexShaderCode.c_str(), "VS", "vs_4_0", &pVSBlob ));
        V_RETURN(gDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &gVertexShader ));
        gContext->VSSetShader( gVertexShader, NULL, 0 );
    }

    {
        CD3D11_BUFFER_DESC desc(sizeof(CBOneFrame), D3D11_BIND_CONSTANT_BUFFER);
        V_RETURN(gDevice->CreateBuffer( &desc, NULL, &gCBOneFrame ));
    }

    V(createShaderAndTexturesFromFile(toyFullPath));

    CD3D11_SAMPLER_DESC sampDesc(D3D11_DEFAULT);
    sampDesc.AddressU = sampDesc.AddressV = sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    V_RETURN(gDevice->CreateSamplerState( &sampDesc, &gSamplerSmooth ));

    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    V_RETURN(gDevice->CreateSamplerState( &sampDesc, &gSamplerBlocky ));

    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = sampDesc.AddressV = sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
    V_RETURN(gDevice->CreateSamplerState( &sampDesc, &gSamplerMirror ));

    // pipeline stats that remain constant
    gContext->RSSetViewports( 1, &g_viewport );
    gContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void DestroyDevice()
{
    if (gContext) gContext->ClearState();

    for (size_t i=0; i<gTextureSRVs.size(); i++)
    {
        SAFE_RELEASE(gTextureSRVs[i]);
    }
}


void DestroyWindow()
{
    if (gHWnd)
        ::DestroyWindow( gHWnd );
    UnregisterClass(kAppName, gHInst);
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

    bool isShaderModified = false;
    std::string newToyFileName = gToyFileName;

    switch( message )
    {
    case WM_TIMER:
        {
            FILETIME ftime = getFileModifyTime(gToyFileName);
            if (ftime.dwLowDateTime != gLastModifyTime.dwLowDateTime || ftime.dwHighDateTime != gLastModifyTime.dwHighDateTime)
            {
                ftime = gLastModifyTime;
                if (gFailsToCompileShader)
                {
                    // HACK: close the previous MessageBox
                    if (HWND hBox = ::FindWindow(NULL, kErrorBoxName))
                    {
                        ::SendMessage(hBox, WM_CLOSE, 0, 0);
                    }
                }
                isShaderModified = true;
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
                    newToyFileName = fileName;
                    isShaderModified = true;
                    break;
                }
            }

            if (!isShaderModified)
            {
                ::MessageBox(gHWnd, "Please drag a shader file (.toy).", kAppName, MB_OK);
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
            gCbOneFrame.mouse.x = (float)mouseX;
            gCbOneFrame.mouse.y = (float)mouseY;
        }break;
    case WM_DESTROY:
        PostQuitMessage( 0 );
        break;
    default:
        return DefWindowProc( hWnd, message, wParam, lParam );
    }

    if (isShaderModified)
    {
        createShaderAndTexturesFromFile(newToyFileName);
        for (int i=0; i<2; i++)
        {
            gContext->ClearRenderTargetView( PingPong::RTVs[i], kBlackColor );
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
    gCbOneFrame.time = ( GetTickCount() - dwTimeStart ) / 1000.0f;

    if (gIsCameraDevice && gVideoInput.isFrameNew(kDeviceId))
    {
        V(updateTextureFromCamera(0, kDeviceId));
    }

    gContext->ClearRenderTargetView( PingPong::RTVs[PingPong::frontBufferIdx], kBlackColor );

    ID3D11RenderTargetView* pRTVs[] = {PingPong::RTVs[PingPong::frontBufferIdx]};
    gContext->OMSetRenderTargets( _countof(pRTVs), pRTVs, NULL );

    gContext->UpdateSubresource( gCBOneFrame, 0, NULL, &gCbOneFrame, 0, 0 );

    ID3D11Buffer* pCBuffers[] = {gCBOneFrame};
    gContext->PSSetConstantBuffers( 0, _countof(pCBuffers), pCBuffers );

    if (!gTextureSRVs.empty())
        gContext->PSSetShaderResources( 0, gTextureSRVs.size(), &gTextureSRVs[0] );
    ID3D11ShaderResourceView* pSRVs[] = {PingPong::SRVs[PingPong::backBufferIdx]};
    gContext->PSSetShaderResources( 1, _countof(pSRVs), pSRVs );

    ID3D11SamplerState* pSamplers[] = {gSamplerSmooth, gSamplerBlocky, gSamplerMirror};
    gContext->PSSetSamplers( 0, _countof(pSamplers), pSamplers );

    gContext->Draw( 3, 0 );

    ID3D11ShaderResourceView* pZeroSRVs[8] = {NULL};
    gContext->PSSetShaderResources( 1, _countof(pZeroSRVs), pZeroSRVs );

    gContext->CopyResource(gBackbuffer, PingPong::TEXs[PingPong::frontBufferIdx]);

    gSwapChain->Present( 0, 0 );

    std::swap(PingPong::frontBufferIdx, PingPong::backBufferIdx);
}

HRESULT updateTextureFromCamera( int textureIdx, int deviceId )
{
    hr = S_OK;

    bool isCreateTexture = false;
    if (gTextureSRVs[textureIdx] == NULL)
    {
       isCreateTexture = true;
    }

    if (isCreateTexture)
    {
        while (!gVideoInput.isFrameNew(kDeviceId))
        {
            ::Sleep(30);
        }
    }

    int srcWidth = gVideoInput.getWidth(deviceId);
    int srcHeight = gVideoInput.getHeight(deviceId);
    BYTE* srcPixels = gVideoInput.getPixels(kDeviceId, true, true);

    // texture size is smaller than camera frame size in order to fix the black border issue
    int dstWidth = srcWidth - 1;
    int dstHeight = srcHeight - 1;

    CComPtr<ID3D11Texture2D> tex;

    if (isCreateTexture)
    {
        CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_R8G8B8A8_UNORM, dstWidth, dstHeight);
        desc.MipLevels = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        V_RETURN(gDevice->CreateTexture2D( &desc, NULL, &tex ));
        V_RETURN(gDevice->CreateShaderResourceView(tex, NULL, &gTextureSRVs[textureIdx]));
    }
    else
    {
        gTextureSRVs[textureIdx]->GetResource(reinterpret_cast<ID3D11Resource**>(&tex)); 
    }

    struct ColorRGB
    {
        BYTE r,g,b;
    };

    struct ColorRGBA
    {
        ColorRGB rgb;
        BYTE a;
    };

    D3D11_MAPPED_SUBRESOURCE mappedRes;
    V_RETURN(gContext->Map(tex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes));
    ColorRGB* src = reinterpret_cast<ColorRGB*>(srcPixels);
    ColorRGBA* dest = reinterpret_cast<ColorRGBA*>(mappedRes.pData);
    UINT pitch = mappedRes.RowPitch / sizeof(ColorRGBA);
    for (int y = 0; y < dstHeight; y++)
    {
        for (int x = 0; x < dstWidth; x++)
        {
            dest[y * pitch + x].rgb = src[y * srcWidth + srcWidth - x];
            dest[y * pitch + x].a = 255;
        }
    }
    gContext->Unmap(tex, 0);

    return hr;
}
