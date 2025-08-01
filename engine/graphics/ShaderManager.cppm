module;

#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <sstream>

export module Graphics.DX12.ShaderManager;

import Core.Assert;
import Core.Types;
import Core.Log;
import Graphics.DX12.Device;
import Graphics.DX12.Pipeline;
import Graphics;
import std;

using Microsoft::WRL::ComPtr;

export class ShaderManager {
private:
    struct ShaderCacheEntry {
        ShaderBytecode bytecode;
        std::filesystem::file_time_type lastModified;
    };

    std::filesystem::path m_ShaderDirectory;
    UnorderedMap<std::string, ShaderCacheEntry> m_Cache;
    Vector<std::filesystem::path> m_IncludePaths;
    bool m_EnableHotReload = true;

    class IncludeHandler : public ID3DInclude {
    private:
        const Vector<std::filesystem::path>* m_IncludePaths;
        Vector<std::string> m_IncludeData;

    public:
        explicit IncludeHandler(const Vector<std::filesystem::path>* paths) : m_IncludePaths{paths} {}

        HRESULT Open(D3D_INCLUDE_TYPE /*includeType*/, LPCSTR pFileName, LPCVOID /*pParentData*/,
                    LPCVOID* ppData, UINT* pBytes) override {

            std::filesystem::path filePath;

            for (const auto& basePath : *m_IncludePaths) {
                auto testPath = basePath / pFileName;
                if (std::filesystem::exists(testPath)) {
                    filePath = testPath;
                    break;
                }
            }

            if (filePath.empty()) {
                Logger::Error("Failed to find include file: {}", pFileName);
                return E_FAIL;
            }

            std::ifstream file(filePath, std::ios::binary | std::ios::ate);
            if (!file.is_open()) {
                Logger::Error("Failed to open include file: {}", filePath.string());
                return E_FAIL;
            }

            auto size = file.tellg();
            file.seekg(0, std::ios::beg);

            m_IncludeData.emplace_back(size, '\0');
            auto& data = m_IncludeData.back();
            file.read(data.data(), size);

            *ppData = data.data();
            *pBytes = static_cast<UINT>(data.size());

            return S_OK;
        }

        HRESULT Close(LPCVOID /*pData*/) override {
            return S_OK;
        }
    };

public:
    explicit ShaderManager(const std::filesystem::path& shaderDirectory = "Shaders")
        : m_ShaderDirectory{shaderDirectory} {

        if (!std::filesystem::exists(m_ShaderDirectory)) {
            Logger::Warn("Shader directory '{}' does not exist", m_ShaderDirectory.string());
        }

        m_IncludePaths.push_back(m_ShaderDirectory);
        m_IncludePaths.push_back(m_ShaderDirectory / "Include");

        Logger::Info("ShaderManager initialized with directory: {}", m_ShaderDirectory.string());
    }

    void AddIncludePath(const std::filesystem::path& path) {
        m_IncludePaths.push_back(path);
        Logger::Debug("Added shader include path: {}", path.string());
    }

    void SetEnableHotReload(bool enable) {
        m_EnableHotReload = enable;
    }

    ShaderBytecode LoadShader(const std::string& filename, const std::string& entryPoint,
                             const std::string& target, const Vector<std::pair<std::string, std::string>>& defines = {}) {

        auto filePath = m_ShaderDirectory / filename;

        std::string cacheKey = GenerateCacheKey(filename, entryPoint, target, defines);

        if (m_EnableHotReload && m_Cache.contains(cacheKey)) {
            auto lastWriteTime = std::filesystem::last_write_time(filePath);
            if (lastWriteTime <= m_Cache[cacheKey].lastModified) {
                Logger::Debug("Using cached shader: {}", filename);
                return m_Cache[cacheKey].bytecode;
            }
            Logger::Info("Shader file modified, recompiling: {}", filename);
        }

        std::string source = ReadShaderFile(filePath);
        if (source.empty()) {
            assert(false, "Failed to read shader file");
        }

        auto bytecode = CompileShaderFromSource(source, filePath.string(), entryPoint, target, defines);

        if (m_EnableHotReload) {
            m_Cache[cacheKey] = {
                bytecode,
                std::filesystem::last_write_time(filePath)
            };
        }

        return bytecode;
    }

    ShaderBytecode CompileShaderFromSource(const std::string& source, const std::string& sourceName,
                                          const std::string& entryPoint, const std::string& target,
                                          const Vector<std::pair<std::string, std::string>>& defines = {}) {

        ComPtr<ID3DBlob> shaderBlob;
        ComPtr<ID3DBlob> errorBlob;

        UINT compileFlags = 0;
#ifdef _DEBUG
        compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_ENABLE_STRICTNESS;
#else
        compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        Vector<D3D_SHADER_MACRO> macros;
        for (const auto& [name, value] : defines) {
            macros.push_back({name.c_str(), value.c_str()});
        }
        macros.push_back({nullptr, nullptr});

        IncludeHandler includeHandler(&m_IncludePaths);

        HRESULT hr = D3DCompile(
            source.c_str(),
            source.size(),
            sourceName.c_str(),
            macros.data(),
            &includeHandler,
            entryPoint.c_str(),
            target.c_str(),
            compileFlags,
            0,
            &shaderBlob,
            &errorBlob
        );

        if (FAILED(hr)) {
            if (errorBlob) {
                std::string errorMsg(static_cast<const char*>(errorBlob->GetBufferPointer()),
                                   errorBlob->GetBufferSize());
                Logger::Error("Shader compilation failed for '{}':\n{}", sourceName, errorMsg);
            }
            assert(false, "Failed to compile shader");
        }

        if (errorBlob && errorBlob->GetBufferSize() > 0) {
            std::string warningMsg(static_cast<const char*>(errorBlob->GetBufferPointer()),
                                 errorBlob->GetBufferSize());
            Logger::Warn("Shader compilation warnings for '{}':\n{}", sourceName, warningMsg);
        }

        ShaderBytecode bytecode;
        bytecode.data.resize(shaderBlob->GetBufferSize());
        std::memcpy(bytecode.data.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());

        Logger::Debug("Successfully compiled shader: {} ({})", sourceName, target);

        return bytecode;
    }

    ShaderBytecode LoadVertexShader(const std::string& filename, const std::string& entryPoint = "VSMain",
                                   const Vector<std::pair<std::string, std::string>>& defines = {}) {
        return LoadShader(filename, entryPoint, "vs_5_1", defines);
    }

    ShaderBytecode LoadPixelShader(const std::string& filename, const std::string& entryPoint = "PSMain",
                                  const Vector<std::pair<std::string, std::string>>& defines = {}) {
        return LoadShader(filename, entryPoint, "ps_5_1", defines);
    }

    ShaderBytecode LoadComputeShader(const std::string& filename, const std::string& entryPoint = "CSMain",
                                    const Vector<std::pair<std::string, std::string>>& defines = {}) {
        return LoadShader(filename, entryPoint, "cs_5_1", defines);
    }

    ShaderBytecode LoadGeometryShader(const std::string& filename, const std::string& entryPoint = "GSMain",
                                     const Vector<std::pair<std::string, std::string>>& defines = {}) {
        return LoadShader(filename, entryPoint, "gs_5_1", defines);
    }

    Vector<std::string> CheckForModifiedShaders() {
        Vector<std::string> modifiedShaders;

        if (!m_EnableHotReload) return modifiedShaders;

        for (const auto& [key, entry] : m_Cache) {
            auto filename = key.substr(0, key.find('|'));
            auto filePath = m_ShaderDirectory / filename;

            if (std::filesystem::exists(filePath)) {
                auto currentTime = std::filesystem::last_write_time(filePath);
                if (currentTime > entry.lastModified) {
                    modifiedShaders.push_back(filename);
                }
            }
        }

        return modifiedShaders;
    }

    void ClearCache() {
        m_Cache.clear();
        Logger::Info("Shader cache cleared");
    }

private:
    std::string ReadShaderFile(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            Logger::Error("Failed to open shader file: {}", path.string());
            return "";
        }

        auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string content(size, '\0');
        file.read(content.data(), size);

        return content;
    }

    std::string GenerateCacheKey(const std::string& filename, const std::string& entryPoint,
                                const std::string& target, const Vector<std::pair<std::string, std::string>>& defines) {
        std::stringstream ss;
        ss << filename << "|" << entryPoint << "|" << target;

        for (const auto& [name, value] : defines) {
            ss << "|" << name << "=" << value;
        }

        return ss.str();
    }
};
