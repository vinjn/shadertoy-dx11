#include "V.h"
#include <d3dcompiler.h>
#include <sstream>

using std::string;
using std::vector;

//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DX11
//--------------------------------------------------------------------------------------
inline HRESULT compileShaderFromFile( LPCSTR szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    HRESULT hr = S_OK;

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* pErrorBlob;
    hr = D3DX11CompileFromFileA( szFileName, NULL, NULL, szEntryPoint, szShaderModel, 
        dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL );
    if( FAILED(hr) )
    {
        if( pErrorBlob != NULL )
            OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        if( pErrorBlob ) pErrorBlob->Release();
        return hr;
    }
    if( pErrorBlob ) pErrorBlob->Release();

    return S_OK;
}

HRESULT compileShaderFromMemory( LPCSTR szText, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, std::string* pErrorStr /*= NULL*/ )
{
    HRESULT hr = S_OK;

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* pErrorBlob;
    hr = D3DX11CompileFromMemory( szText, strlen(szText), 
        NULL, NULL, NULL,
        szEntryPoint, szShaderModel, 
        dwShaderFlags, 0, NULL, 
        ppBlobOut, &pErrorBlob, 
        NULL );
    if( FAILED(hr) )
    {
        if( pErrorBlob != NULL )
        {
            if (pErrorStr == NULL)
            {
                OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
            }
            else
            {
                *pErrorStr = (char*)pErrorBlob->GetBufferPointer();
            }
        }
        if( pErrorBlob ) pErrorBlob->Release();
        return hr;
    }
    if( pErrorBlob ) pErrorBlob->Release();

    return S_OK;
}

FILETIME getFileModifyTime( const std::string& filename )
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


std::string getOpenFilePath( HWND hWnd, const std::string &initialPath, std::vector<std::string> extensions )
{
    OPENFILENAMEA ofn;       // common dialog box structure
    char szFile[260];       // buffer for file name

    // Initialize OPENFILENAME
    ::ZeroMemory( &ofn, sizeof(ofn) );
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
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

        strcpy( extensionStr, "HlslShaderToy file (.toy)" );
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

#include <Urlmon.h>
#pragma comment(lib, "Urlmon.lib")

HRESULT downloadFromUrl( const std::string& url, const std::string& localPath)
{
    HRESULT res = ::URLDownloadToFile(NULL, url.c_str(), localPath.c_str(), 0, NULL);

#define MSG_BOX_URL(msg) ::MessageBox(NULL, msg, "URLDownloadToFile", MB_OK)

    if(res == S_OK) {
        //OutputDebugStringA("Ok\n");
    } else if(res == E_OUTOFMEMORY) {
        MSG_BOX_URL("Buffer length invalid, or insufficient memory");
    } else if(res == INET_E_DOWNLOAD_FAILURE) {
        MSG_BOX_URL("INET_E_DOWNLOAD_FAILURE");
    } else {
        std::stringstream ss;
        ss << "Something wrong with URL:\n" << url;
        MSG_BOX_URL(ss.str().c_str());
    }

#undef MSG_BOX_URL

    return res;
}

std::string getTempFolder()
{
    DWORD result = ::GetTempPath( 0, "" );
    if( ! result )
        throw std::runtime_error("Could not get system temp path");

    std::vector<char> tempPath(result + 1);
    result = ::GetTempPath(static_cast<DWORD>(tempPath.size()), &tempPath[0]);
    if( ( ! result ) || ( result >= tempPath.size() ) )
        throw std::runtime_error("Could not get system temp path");

    return std::string( tempPath.begin(), tempPath.begin() + static_cast<std::size_t>(result) );
}

std::string extractToyFromGithubIssuse( const std::string& localHtmlFile )
{
    return 
        "// demonstrate the use of url image\n"
        "// https://f.cloud.github.com/assets/558657/590903/5b36a0e0-c9f3-11e2-93f0-743a1469c0d9.png\n"
        " \n"
        "float4 main( float4 pos : SV_POSITION) : SV_Target\n"
        "{\n"
        "    return textures[0].Sample( smooth, pos.xy / resolution );\n"
        "}\n"
        ;
}
