#pragma once

#include "video_encoder.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <d3d11.h>
#include <wrl/client.h>

// Tell nvEncodeAPI not to include d3d9/d3d10
#define NVENCAPI_NO_D3D9
#define NVENCAPI_NO_D3D10
#include <nvEncodeAPI.h>

namespace opendriver::driver::video {

class NvencEncoder : public IVideoEncoder {
public:
    NvencEncoder();
    ~NvencEncoder() override;

    void SetLogger(std::function<void(const char*)> logger) override;
    bool Initialize(const VideoConfig& config) override;
    void Shutdown() override;
    bool EncodeFrame(void* texture_handle, std::vector<uint8_t>& out_packet) override;
    std::string GetLastError() const override;

private:
    void Log(const std::string& msg);
    void SetLastError(const std::string& err);
    bool LoadNvencLibrary();
    bool InitializeD3D11();
    bool OpenSession(const VideoConfig& config);
    bool AllocateBuffers();
    void ReleaseBuffers();
    void FlushEncoder(std::vector<uint8_t>& out_packet);

    std::function<void(const char*)> m_logger;
    std::string m_lastError;
    
    // Dynamic loading of nvEncodeAPI64.dll
    HMODULE m_hNvencLib = nullptr;
    NV_ENCODE_API_FUNCTION_LIST m_nvenc = {0};

    Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3dContext;

    void* m_encoderSession = nullptr;
    
    // For zero-copy, we need to register the D3D11 texture
    // and create a bitstream buffer for output.
    NV_ENC_REGISTER_RESOURCE m_registeredResource = {0};
    NV_ENC_MAP_INPUT_RESOURCE m_mappedResource = {0};
    NV_ENC_CREATE_BITSTREAM_BUFFER m_bitstreamBuffer = {0};
    
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_initialized = false;
};

} // namespace opendriver::driver::video
