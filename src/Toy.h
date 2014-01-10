#include <windows.h>
#include <windowsx.h>
#include <xnamath.h>
#include <atlbase.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include "V.h"
#include "videoInput/videoInput.h"

const char kAppName[] = "shadertoy-dx11";
const char kErrorBoxName[] = "Shader Compiling Error";
// 640 X 480 is preferred by my notebook
// TODO: make it an option
const int kAppWidth = 640;
const int kAppHeight = 480;
const int kFileChangeDetectionMS = 1000;
const float kBlackColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // red, green, blue, alpha

extern CComPtr<ID3D11Device>                    gDevice;
extern CComPtr<ID3D11DeviceContext>             gContext;
extern  HWND gHWnd;
extern CComPtr<ID3D11PixelShader>               gPixelShader;
extern std::vector<ID3D11ShaderResourceView*>   gTextureSRVs;      // Texture2D textures[];

extern std::string                              gToyFileName;
extern FILETIME                                 gLastModifyTime;
extern bool                                     gFailsToCompileShader;
extern bool                                     gNeesToOutputCompleteHlsl;

const int kDeviceId = 0; // TODO: support custom id
extern bool                                     gIsCameraDevice;    // must be used as textures[0]
extern videoInput                               gVideoInput;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT SetupWindow( HINSTANCE hInstance, int nCmdShow );
void    DestroyWindow();
HRESULT SetupDevice( const std::string& filename );
void    DestroyDevice();
LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );
void Render();

HRESULT createShaderAndTexturesFromFile(const std::string& filename);
HRESULT updateTextureFromCamera(int textureIdx, int deviceId);