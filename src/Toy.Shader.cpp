#include <windows.h>
#include <windowsx.h>
#include <xnamath.h>
#include <atlbase.h>
#include <fstream>
#include <regex>
#include "Toy.h"

#if _MSC_VER < 1600
namespace std
{
    using namespace tr1;
}
#endif // _MSC_VER

namespace
{
    bool isUrlPath( const std::string &possiblePath ) 
    {
        return possiblePath.find("http://") == 0 || 
            possiblePath.find("https://") == 0 ||
            possiblePath.find("ftp://") == 0;
    }

    bool isGithubIssuePath( const std::string &possiblePath ) 
    {
        return possiblePath.find("/issues/") != std::string::npos;
    }

    std::string getLeafName( const std::string &toyFullPath ) 
    {
        char buffer[MAX_PATH];
        strcpy_s(buffer, MAX_PATH, toyFullPath.c_str());
        ::PathStripPath(buffer);
        return buffer;
    }
}

// TODO: tear down to small functions
HRESULT createShaderAndTexturesFromFile(const std::string& toyFullPath)
{
    HRESULT hr = S_OK;

    // "e:\__svn_pool\HlslShaderToy\media\HelloWorld.toy" -> "HelloWorld.toy"
    std::string toyLeafName = getLeafName(toyFullPath);

    // "e:\__svn_pool\HlslShaderToy\media\HelloWorld.toy" -> "e:\__svn_pool\HlslShaderToy\media\"
    // for url, this value would be ""
    std::string toyFolderName;
    {
        std::string::size_type pos = toyFullPath.rfind(toyLeafName);
        if (pos != std::string::npos)
        {
            toyFolderName = toyFullPath.substr(0, pos);
        }
        else
        {
            ::MessageBox(gHWnd, "Invalid filename.", kAppName, MB_OK);
        }
    }

    std::stringstream ss;
    ss << "Opening shader file: " << toyFullPath << std::endl;
    ::OutputDebugStringA(ss.str().c_str());

    if (isUrlPath(toyFullPath))
    {
        // https://raw.github.com/vinjn/HlslShaderToy/master/samples/HelloWorldUrl.toy -> C:/temp/network_HelloWorldUrl.toy.toy
        std::string url = toyFullPath;
        std::stringstream ss;
        ss << getTempFolder() << "network_" << toyLeafName << ".toy";

        std::string localToyPath = ss.str();

        V_RETURN(downloadFromUrl(url, localToyPath));

        if (isGithubIssuePath(url))
        {
            std::ofstream ofs(localToyPath.c_str());
            std::string toyContent = extractToyFromGithubIssuse(localToyPath);
            ofs << toyContent;
        }

        return createShaderAndTexturesFromFile(localToyPath);
    }

    std::ifstream ifs(toyFullPath.c_str());
    if (!ifs)
    {
        return D3D11_ERROR_FILE_NOT_FOUND;
    }

    gLastModifyTime = getFileModifyTime(toyFullPath);
    gToyFileName = toyFullPath;

    std::vector<std::string> texturePaths;
    const std::regex reResourceLocations("[^/]*//\\s*(.*)");
    // e:\\__svn_pool\\HlslShaderToy\\media\\ducky.png
    // https://raw.github.com/vinjn/HlslShaderToy/master/media/ducky.png

    gIsCameraDevice = false;

    std::stringstream toyShaderText;
    std::string oneline;
    while (std::getline(ifs, oneline))
    {
        toyShaderText << oneline << "\n";

        std::smatch sm;
        if (std::regex_match(oneline, sm, reResourceLocations))
        {
            std::string possiblePath = sm.str(1);

            if (possiblePath == "camera")
            {
                gIsCameraDevice = true;
                if (!gVideoInput.setupDevice(kDeviceId))
                {
                    // fillFakeData(gCameraFrameData);
                    possiblePath = "fake camera device";
                }  
                texturePaths.push_back(possiblePath);
                std::stringstream ss;
                ss << "Loading image from: " << possiblePath <<"\n";
                OutputDebugStringA(ss.str().c_str());

                continue;
            }

            if (isUrlPath(possiblePath) )
            {
                // change possiblePath
                // https://raw.github.com/vinjn/HlslShaderToy/master/media/ducky.png -> C:/temp/HelloWorld.toy_ducky.png
                std::string url = possiblePath;
                std::string imageLeafName = getLeafName(url);
                std::stringstream ss;
                ss << getTempFolder() << toyLeafName << "_" << imageLeafName;

                possiblePath = ss.str();

                V_RETURN(downloadFromUrl(url, possiblePath));
            }
            else if (::PathIsRelative(possiblePath.c_str()))
            {
                // change possiblePath to absolute path
                possiblePath = toyFolderName + possiblePath;
            }

            if (std::ifstream(possiblePath.c_str()))
            {
                D3DX11_IMAGE_INFO imageInfo;
                if (SUCCEEDED(D3DX11GetImageInfoFromFile(possiblePath.c_str(), NULL, &imageInfo, NULL)))
                {
                    texturePaths.push_back(possiblePath);
                    std::stringstream ss;
                    ss << "Loading image from: " << possiblePath <<"\n";
                    OutputDebugStringA(ss.str().c_str());
                }
            }
        }
    }
    ifs.close();

    std::string pixelShaderHeader;
    {
        std::stringstream pixelShaderHeaderSS;
        if (!texturePaths.empty())
        {
            pixelShaderHeaderSS << "Texture2D textures[" << texturePaths.size() <<"] : register( t0 );\n";
        }

        pixelShaderHeaderSS <<        
            "Texture2D backbuffer : register( t1 );\n"
            "\n"
            "SamplerState smooth : register( s0 );\n"
            "SamplerState blocky : register( s1 );\n"
            "SamplerState mirror : register( s2 );\n"
            "\n"
            "cbuffer CBOneFrame : register( b0 )\n"
            "{\n"
            "    float2     resolution;     // viewport resolution (in pixels)\n"
            "    float      time;           // shader playback time (in seconds)\n"
            "    float      aspect;         // cached aspect of viewport\n"
            "    float4     mouse;          // mouse pixel coords. xy: current (if MLB down), zw: click\n"
            "    float4     date;           // (year, month, day, time in seconds)\n"
            "};\n"
            "\n"
            ;

        pixelShaderHeader = pixelShaderHeaderSS.str();
    }
    const size_t nCommonShaderNewLines = std::count(pixelShaderHeader.begin(), pixelShaderHeader.end(), '\n');

    // add together
    std::string psText = pixelShaderHeader + toyShaderText.str();

    // output complete shader file
    if (gNeesToOutputCompleteHlsl)
    {
        std::ofstream of((toyFullPath+".hlsl").c_str());
        if (of)
        {
            of << psText;
        }
    }

    ID3DBlob* pPSBlob = NULL;
    std::string errorMsg;
    hr = compileShaderFromMemory( psText.c_str(), "main", "ps_4_0", &pPSBlob, &errorMsg );

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
            std::istringstream(lineStr) >> lineNo;
            lineNo -= nCommonShaderNewLines;
            std::ostringstream ss;
            ss << lineNo;
            errorMsg.replace(posCarriageReturn+1, length, ss.str());

            posCarriageReturn = errorMsg.find(10, posComma);     
            if (posCarriageReturn == std::string::npos)
                break;
            posCarriageReturn ++;
        }

        gFailsToCompileShader = true;
        OutputDebugStringA(errorMsg.c_str());
        ::MessageBox(gHWnd, errorMsg.c_str(), kErrorBoxName, MB_OK);

        return E_FAIL;
    }

    gFailsToCompileShader = false;
    V_RETURN(gDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &gPixelShader ));
    gContext->PSSetShader( gPixelShader, NULL, 0 );

    std::stringstream titleSS;
    titleSS << toyFullPath << " - " << kAppName;
    ::SetWindowText(gHWnd, titleSS.str().c_str());

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

    for (size_t i=0; i<gTextureSRVs.size(); i++)
    {
        SAFE_RELEASE(gTextureSRVs[i]);
    }

    gTextureSRVs.resize(texturePaths.size());
    for (size_t i=0; i<texturePaths.size(); i++)
    {
        if (gIsCameraDevice && i == 0)
        {
            V_RETURN(updateTextureFromCamera(i, kDeviceId));
            continue;
        }
        V_RETURN(D3DX11CreateShaderResourceViewFromFile( gDevice, texturePaths[i].c_str(), NULL, NULL, &gTextureSRVs[i], NULL ));
    }

    ::Beep( rand()%100+200, 300 );

    return hr;
}
