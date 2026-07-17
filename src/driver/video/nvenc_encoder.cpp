#include "nvenc_encoder.h"

#if defined(_WIN32) || defined(WIN32)

#include <windows.h>
#include <d3d11_1.h>
#include <iostream>
#include <sstream>

namespace opendriver::driver::video {

NvencEncoder::NvencEncoder() {
    m_nvenc.version = NV_ENCODE_API_FUNCTION_LIST_VER;
}

NvencEncoder::~NvencEncoder() {
    Shutdown();
}

void NvencEncoder::SetLogger(std::function<void(const char*)> logger) {
    m_logger = logger;
}

void NvencEncoder::Log(const std::string& msg) {
    if (m_logger) {
        m_logger(msg.c_str());
    }
}

void NvencEncoder::SetLastError(const std::string& err) {
    m_lastError = err;
    Log("NvencEncoder Error: " + err);
}

std::string NvencEncoder::GetLastError() const {
    return m_lastError;
}

bool NvencEncoder::LoadNvencLibrary() {
    if (m_hNvencLib) return true;

    m_hNvencLib = LoadLibraryA("nvEncodeAPI64.dll");
    if (!m_hNvencLib) {
        m_hNvencLib = LoadLibraryA("nvEncodeAPI.dll");
    }

    if (!m_hNvencLib) {
        SetLastError("Failed to load nvEncodeAPI64.dll. NVIDIA drivers may not be installed or supported.");
        return false;
    }

    typedef NVENCSTATUS(NVENCAPI * PNVENCODEAPIGETMAXSUPPORTEDVERSION)(uint32_t*);
    typedef NVENCSTATUS(NVENCAPI * PNVENCODEAPICREATEINSTANCE)(NV_ENCODE_API_FUNCTION_LIST*);

    auto NvEncodeAPIGetMaxSupportedVersion = (PNVENCODEAPIGETMAXSUPPORTEDVERSION)GetProcAddress(m_hNvencLib, "NvEncodeAPIGetMaxSupportedVersion");
    auto NvEncodeAPICreateInstance = (PNVENCODEAPICREATEINSTANCE)GetProcAddress(m_hNvencLib, "NvEncodeAPICreateInstance");

    if (!NvEncodeAPIGetMaxSupportedVersion || !NvEncodeAPICreateInstance) {
        SetLastError("Failed to get NVENC function pointers.");
        return false;
    }

    uint32_t version = 0;
    uint32_t currentVersion = (NVENCAPI_MAJOR_VERSION << 4) | NVENCAPI_MINOR_VERSION;
    NvEncodeAPIGetMaxSupportedVersion(&version);

    if (version < currentVersion) {
        Log("NvencEncoder Warning: NVIDIA driver version (" + std::to_string(version) + ") is older than compiled SDK (" + std::to_string(currentVersion) + "). NVENC initialization might fail, but proceeding anyway.");
    }

    if (NvEncodeAPICreateInstance(&m_nvenc) != NV_ENC_SUCCESS) {
        SetLastError("Failed to create NVENC instance.");
        return false;
    }

    return true;
}

bool NvencEncoder::InitializeD3D11() {
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags,
        featureLevels, 2, D3D11_SDK_VERSION, &m_d3dDevice, &featureLevel, &m_d3dContext);

    if (FAILED(hr)) {
        SetLastError("Failed to create D3D11 device for NVENC.");
        return false;
    }
    return true;
}

bool NvencEncoder::OpenSession(const VideoConfig& config) {
    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS sessionParams = { NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER };
    sessionParams.device = m_d3dDevice.Get();
    sessionParams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
    sessionParams.apiVersion = NVENCAPI_VERSION;
    
    if (m_nvenc.nvEncOpenEncodeSessionEx(&sessionParams, &m_encoderSession) != NV_ENC_SUCCESS) {
        SetLastError("nvEncOpenEncodeSessionEx failed.");
        return false;
    }

    NV_ENC_INITIALIZE_PARAMS initParams = { NV_ENC_INITIALIZE_PARAMS_VER };
    NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
    initParams.encodeConfig = &encodeConfig;

    initParams.encodeGUID = NV_ENC_CODEC_H264_GUID;
    initParams.presetGUID = NV_ENC_PRESET_P1_GUID;  // P1 = lowest latency
    initParams.encodeWidth = config.width;
    initParams.encodeHeight = config.height;
    initParams.darWidth = config.width;
    initParams.darHeight = config.height;
    initParams.frameRateNum = (uint32_t)config.refresh_rate;
    initParams.frameRateDen = 1;
    initParams.enablePTD = 1;
    initParams.reportSliceOffsets = 0;
    initParams.tuningInfo = NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY;

    NV_ENC_PRESET_CONFIG presetConfig = { NV_ENC_PRESET_CONFIG_VER, { NV_ENC_CONFIG_VER } };
    if (m_nvenc.nvEncGetEncodePresetConfigEx(m_encoderSession, initParams.encodeGUID, initParams.presetGUID, initParams.tuningInfo, &presetConfig) != NV_ENC_SUCCESS) {
        SetLastError("nvEncGetEncodePresetConfigEx failed.");
        return false;
    }

    memcpy(&encodeConfig, &presetConfig.presetCfg, sizeof(NV_ENC_CONFIG));
    
    encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
    encodeConfig.rcParams.averageBitRate = config.bitrate_mbps * 1000000;
    encodeConfig.rcParams.maxBitRate = config.bitrate_mbps * 1000000;
    encodeConfig.gopLength = NVENC_INFINITE_GOPLENGTH;
    encodeConfig.frameIntervalP = 1;
    encodeConfig.encodeCodecConfig.h264Config.idrPeriod = (uint32_t)config.refresh_rate * 2;
    encodeConfig.encodeCodecConfig.h264Config.repeatSPSPPS = 1;

    if (m_nvenc.nvEncInitializeEncoder(m_encoderSession, &initParams) != NV_ENC_SUCCESS) {
        SetLastError("nvEncInitializeEncoder failed.");
        return false;
    }

    return true;
}

bool NvencEncoder::AllocateBuffers() {
    m_bitstreamBuffer.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
    if (m_nvenc.nvEncCreateBitstreamBuffer(m_encoderSession, &m_bitstreamBuffer) != NV_ENC_SUCCESS) {
        SetLastError("nvEncCreateBitstreamBuffer failed.");
        return false;
    }
    return true;
}

void NvencEncoder::ReleaseBuffers() {
    if (m_encoderSession) {
        if (m_mappedResource.mappedResource) {
            m_nvenc.nvEncUnmapInputResource(m_encoderSession, m_mappedResource.mappedResource);
            m_mappedResource.mappedResource = nullptr;
        }
        if (m_registeredResource.registeredResource) {
            m_nvenc.nvEncUnregisterResource(m_encoderSession, m_registeredResource.registeredResource);
            m_registeredResource.registeredResource = nullptr;
        }
        if (m_bitstreamBuffer.bitstreamBuffer) {
            m_nvenc.nvEncDestroyBitstreamBuffer(m_encoderSession, m_bitstreamBuffer.bitstreamBuffer);
            m_bitstreamBuffer.bitstreamBuffer = nullptr;
        }
    }
}

bool NvencEncoder::Initialize(const VideoConfig& config) {
    if (m_initialized) return true;

    if (!LoadNvencLibrary()) return false;
    if (!InitializeD3D11()) return false;
    if (!OpenSession(config)) return false;
    if (!AllocateBuffers()) return false;

    m_width = config.width;
    m_height = config.height;
    m_initialized = true;

    Log("NVENC H264 initialized successfully: " + std::to_string(m_width) + "x" + std::to_string(m_height));
    return true;
}

void NvencEncoder::Shutdown() {
    if (!m_initialized) return;

    ReleaseBuffers();

    if (m_encoderSession) {
        m_nvenc.nvEncDestroyEncoder(m_encoderSession);
        m_encoderSession = nullptr;
    }

    m_d3dContext.Reset();
    m_d3dDevice.Reset();

    if (m_hNvencLib) {
        FreeLibrary(m_hNvencLib);
        m_hNvencLib = nullptr;
    }

    m_initialized = false;
}

bool NvencEncoder::EncodeFrame(void* texture_handle, std::vector<uint8_t>& out_packet) {
    if (!m_initialized || !texture_handle) return false;
    
    Microsoft::WRL::ComPtr<ID3D11Texture2D> shared_texture;
    HRESULT hr = m_d3dDevice->OpenSharedResource(
        reinterpret_cast<HANDLE>(texture_handle),
        __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(shared_texture.GetAddressOf()));

    if (FAILED(hr) || !shared_texture) {
        Microsoft::WRL::ComPtr<ID3D11Device1> d3dDevice1;
        if (SUCCEEDED(m_d3dDevice.As(&d3dDevice1))) {
            hr = d3dDevice1->OpenSharedResource1(
                reinterpret_cast<HANDLE>(texture_handle),
                __uuidof(ID3D11Texture2D),
                reinterpret_cast<void**>(shared_texture.ReleaseAndGetAddressOf()));
        }
        if (FAILED(hr) || !shared_texture) {
            SetLastError("Failed to open shared texture in NVENC.");
            return false;
        }
    }

    NV_ENC_REGISTER_RESOURCE regRes = { NV_ENC_REGISTER_RESOURCE_VER };
    regRes.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
    regRes.resourceToRegister = shared_texture.Get();
    regRes.width = m_width;
    regRes.height = m_height;
    regRes.pitch = 0;
    regRes.bufferFormat = NV_ENC_BUFFER_FORMAT_ABGR;
    regRes.bufferUsage = NV_ENC_INPUT_IMAGE;
    
    if (m_nvenc.nvEncRegisterResource(m_encoderSession, &regRes) != NV_ENC_SUCCESS) {
        SetLastError("nvEncRegisterResource failed.");
        return false;
    }

    NV_ENC_MAP_INPUT_RESOURCE mapRes = { NV_ENC_MAP_INPUT_RESOURCE_VER };
    mapRes.registeredResource = regRes.registeredResource;
    if (m_nvenc.nvEncMapInputResource(m_encoderSession, &mapRes) != NV_ENC_SUCCESS) {
        m_nvenc.nvEncUnregisterResource(m_encoderSession, regRes.registeredResource);
        SetLastError("nvEncMapInputResource failed.");
        return false;
    }

    NV_ENC_PIC_PARAMS picParams = { NV_ENC_PIC_PARAMS_VER };
    picParams.inputWidth = m_width;
    picParams.inputHeight = m_height;
    picParams.inputPitch = m_width;  // Pitch unknown for D3D11 — use width
    picParams.inputBuffer = mapRes.mappedResource;
    picParams.outputBitstream = m_bitstreamBuffer.bitstreamBuffer;
    picParams.bufferFmt = mapRes.mappedBufferFmt;
    picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;

    NVENCSTATUS encStatus = m_nvenc.nvEncEncodePicture(m_encoderSession, &picParams);
    
    bool success = false;
    if (encStatus == NV_ENC_SUCCESS) {
        NV_ENC_LOCK_BITSTREAM lockBitstream = { NV_ENC_LOCK_BITSTREAM_VER };
        lockBitstream.outputBitstream = m_bitstreamBuffer.bitstreamBuffer;
        
        if (m_nvenc.nvEncLockBitstream(m_encoderSession, &lockBitstream) == NV_ENC_SUCCESS) {
            out_packet.assign((uint8_t*)lockBitstream.bitstreamBufferPtr, (uint8_t*)lockBitstream.bitstreamBufferPtr + lockBitstream.bitstreamSizeInBytes);
            m_nvenc.nvEncUnlockBitstream(m_encoderSession, m_bitstreamBuffer.bitstreamBuffer);
            success = true;
        } else {
            SetLastError("nvEncLockBitstream failed.");
        }
    } else if (encStatus == NV_ENC_ERR_NEED_MORE_INPUT) {
        success = true;
    } else {
        SetLastError("nvEncEncodePicture failed.");
    }

    m_nvenc.nvEncUnmapInputResource(m_encoderSession, mapRes.mappedResource);
    m_nvenc.nvEncUnregisterResource(m_encoderSession, regRes.registeredResource);

    return success;
}

std::unique_ptr<IVideoEncoder> CreateNvencEncoder() {
    return std::make_unique<NvencEncoder>();
}

} // namespace opendriver::driver::video

#endif
