#include "MaxineFilterBackend.h"

#include "MaxineTextureBridge.h"

#include "core/FilterEffectIds.h"
#include "core/FilterParamSchema.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QVector>

#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#ifdef PVJ_MAXINE_SDK_LINKED

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include <nvVideoEffects.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if defined(Q_OS_WIN)
using CudaError_t = int;
constexpr CudaError_t kCudaSuccess = 0;

struct CudaRuntimeApi {
    CudaError_t (*malloc)(void**, size_t) = nullptr;
    CudaError_t (*free)(void*) = nullptr;
    CudaError_t (*memset)(void*, int, size_t) = nullptr;
    CudaError_t (*memsetAsync)(void*, int, size_t, void*) = nullptr;
    CudaError_t (*deviceSynchronize)() = nullptr;
    CudaError_t (*setDevice)(int) = nullptr;
    CudaError_t (*deviceGetAttribute)(int*, int, int) = nullptr;
    bool loaded = false;
};

static CudaRuntimeApi& cudaApi()
{
    static CudaRuntimeApi api;
    if (api.loaded) {
        return api;
    }
    api.loaded = true;
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString dllPath = exeDir + QStringLiteral("/cudart64_12.dll");
    HMODULE mod = LoadLibraryW(reinterpret_cast<const wchar_t*>(dllPath.utf16()));
    if (!mod) {
        mod = LoadLibraryW(L"cudart64_12.dll");
    }
    if (!mod) {
        return api;
    }
    api.malloc = reinterpret_cast<CudaError_t (*)(void**, size_t)>(GetProcAddress(mod, "cudaMalloc"));
    api.free = reinterpret_cast<CudaError_t (*)(void*)>(GetProcAddress(mod, "cudaFree"));
    api.memset = reinterpret_cast<CudaError_t (*)(void*, int, size_t)>(GetProcAddress(mod, "cudaMemset"));
    api.memsetAsync = reinterpret_cast<CudaError_t (*)(void*, int, size_t, void*)>(
        GetProcAddress(mod, "cudaMemsetAsync"));
    api.deviceSynchronize =
        reinterpret_cast<CudaError_t (*)()>(GetProcAddress(mod, "cudaDeviceSynchronize"));
    api.setDevice = reinterpret_cast<CudaError_t (*)(int)>(GetProcAddress(mod, "cudaSetDevice"));
    api.deviceGetAttribute = reinterpret_cast<CudaError_t (*)(int*, int, int)>(
        GetProcAddress(mod, "cudaDeviceGetAttribute"));
    return api;
}

static int detectComputeCapability()
{
#if defined(Q_OS_WIN)
    CudaRuntimeApi& cuda = cudaApi();
    if (!cuda.deviceGetAttribute) {
        return 0;
    }
    int major = 0, minor = 0;
    if (cuda.deviceGetAttribute(&major, 75 /*cudaDevAttrComputeCapabilityMajor*/, 0) != kCudaSuccess
        || cuda.deviceGetAttribute(&minor, 76 /*cudaDevAttrComputeCapabilityMinor*/, 0) != kCudaSuccess) {
        return 0;
    }
    return major * 10 + minor;
#else
    return 0;
#endif
}

static bool initCudaDevice()
{
#if defined(Q_OS_WIN)
    CudaRuntimeApi& cuda = cudaApi();
    if (!cuda.setDevice) {
        return false;
    }
    return cuda.setDevice(0) == kCudaSuccess;
#else
    return false;
#endif
}

static void syncCudaDevice()
{
#if defined(Q_OS_WIN)
    if (cudaApi().deviceSynchronize) {
        cudaApi().deviceSynchronize();
    }
#endif
}
#endif

// NVVideoEffectsProxy.cpp (SDK) expects this symbol at global scope.
char* g_nvVFXSDKPath = nullptr;

namespace pvj::render {

struct EffectSlot {
    NvVFX_Handle handle = nullptr;
    bool loaded = false;
    QSize lastInput;
    QSize lastOutput;
    NvVFX_StateObjectHandle denoiseState = nullptr;
    NvCVImage srcCpu{};
    NvCVImage dstCpu{};
    NvCVImage srcGpu{};
    NvCVImage dstGpu{};
    NvCVImage tmpGpu{};
};

static void freeDenoiseState(EffectSlot& slot);

static void freeGpuBuffers(EffectSlot& slot)
{
    NvCVImage_Dealloc(&slot.srcCpu);
    NvCVImage_Dealloc(&slot.dstCpu);
    NvCVImage_Dealloc(&slot.srcGpu);
    NvCVImage_Dealloc(&slot.dstGpu);
    NvCVImage_Dealloc(&slot.tmpGpu);
    slot.srcCpu = {};
    slot.dstCpu = {};
    slot.srcGpu = {};
    slot.dstGpu = {};
    slot.tmpGpu = {};
}

static void invalidateEffectSlot(EffectSlot& slot)
{
    slot.loaded = false;
    freeDenoiseState(slot);
    freeGpuBuffers(slot);
    slot.lastInput = {};
    slot.lastOutput = {};
    syncCudaDevice();
}

static void resetEffectSlot(EffectSlot& slot)
{
    slot.loaded = false;
    freeDenoiseState(slot);
    freeGpuBuffers(slot);
    if (slot.handle) {
        NvVFX_DestroyEffect(slot.handle);
        slot.handle = nullptr;
    }
    slot.lastInput = {};
    slot.lastOutput = {};
}

static bool ensurePlanarGpuBuffers(EffectSlot& slot, int inW, int inH, int outW, int outH)
{
    const QSize inSize(inW, inH);
    const QSize outSize(outW, outH);
    if (slot.lastInput == inSize && slot.lastOutput == outSize && slot.srcGpu.pixels) {
        return true;
    }
    freeGpuBuffers(slot);
    NvCV_Status st = NvCVImage_Alloc(&slot.srcCpu, inW, inH, NVCV_BGR, NVCV_U8, NVCV_CHUNKY, NVCV_CPU, 1);
    if (st != NVCV_SUCCESS) {
        goto fail;
    }
    st = NvCVImage_Alloc(&slot.dstCpu, outW, outH, NVCV_BGR, NVCV_U8, NVCV_CHUNKY, NVCV_CPU, 1);
    if (st != NVCV_SUCCESS) {
        goto fail;
    }
    st = NvCVImage_Alloc(&slot.srcGpu, inW, inH, NVCV_BGR, NVCV_F32, NVCV_PLANAR, NVCV_GPU, 1);
    if (st != NVCV_SUCCESS) {
        goto fail;
    }
    st = NvCVImage_Alloc(&slot.dstGpu, outW, outH, NVCV_BGR, NVCV_F32, NVCV_PLANAR, NVCV_GPU, 1);
    if (st != NVCV_SUCCESS) {
        goto fail;
    }
    st = NvCVImage_Alloc(&slot.tmpGpu, outW, outH, NVCV_BGR, NVCV_F32, NVCV_PLANAR, NVCV_GPU, 0);
    if (st != NVCV_SUCCESS) {
        goto fail;
    }
    st = NvCVImage_Realloc(&slot.tmpGpu, inW, inH, NVCV_BGR, NVCV_F32, NVCV_PLANAR, NVCV_GPU, 0);
    if (st != NVCV_SUCCESS) {
        goto fail;
    }
    slot.lastInput = inSize;
    slot.lastOutput = outSize;
    return true;
fail:
    slot.lastInput = {};
    slot.lastOutput = {};
    freeGpuBuffers(slot);
    return false;
}

static void freeDenoiseState(EffectSlot& slot)
{
    if (slot.denoiseState && slot.handle) {
        NvVFX_DeallocateState(slot.handle, slot.denoiseState);
        slot.denoiseState = nullptr;
    } else {
        slot.denoiseState = nullptr;
    }
}

static bool checkScaleIsotropy(int inW, int inH, int outW, int outH)
{
    return inW > 0 && inH > 0 && outW > 0 && outH > 0
        && (static_cast<qint64>(inW) * outH == static_cast<qint64>(inH) * outW);
}

static bool ensureUpscaleGpuBuffers(EffectSlot& slot, int inW, int inH, int outW, int outH)
{
    const QSize inSize(inW, inH);
    const QSize outSize(outW, outH);
    if (slot.lastInput == inSize && slot.lastOutput == outSize && slot.srcGpu.pixels) {
        return true;
    }
    freeGpuBuffers(slot);
    NvCV_Status st = NvCVImage_Alloc(&slot.srcCpu, inW, inH, NVCV_RGBA, NVCV_U8, NVCV_CHUNKY, NVCV_CPU, 1);
    if (st != NVCV_SUCCESS) {
        goto fail;
    }
    st = NvCVImage_Alloc(&slot.dstCpu, outW, outH, NVCV_RGBA, NVCV_U8, NVCV_CHUNKY, NVCV_CPU, 1);
    if (st != NVCV_SUCCESS) {
        goto fail;
    }
    st = NvCVImage_Alloc(&slot.srcGpu, inW, inH, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 32);
    if (st != NVCV_SUCCESS) {
        goto fail;
    }
    st = NvCVImage_Alloc(&slot.dstGpu, outW, outH, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 32);
    if (st != NVCV_SUCCESS) {
        goto fail;
    }
    st = NvCVImage_Alloc(&slot.tmpGpu, outW, outH, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 0);
    if (st != NVCV_SUCCESS) {
        goto fail;
    }
    st = NvCVImage_Realloc(&slot.tmpGpu, inW, inH, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 0);
    if (st != NVCV_SUCCESS) {
        goto fail;
    }
    slot.lastInput = inSize;
    slot.lastOutput = outSize;
    return true;
fail:
    slot.lastInput = {};
    slot.lastOutput = {};
    freeGpuBuffers(slot);
    return false;
}

static bool g_maxineRuntimeAvailable = false;
static bool g_maxineRuntimeProbed = false;
static QByteArray g_nvVfxDllDirectoryStorage;

#if defined(Q_OS_WIN)
static QString defaultMaxineRuntimeDirectory()
{
    const QString exeDir = QCoreApplication::applicationDirPath();
    if (QFileInfo::exists(exeDir + QStringLiteral("/NVVideoEffects.dll"))) {
        return exeDir;
    }
    const QString env = qEnvironmentVariable("NV_VIDEO_EFFECTS_PATH");
    if (!env.isEmpty() && env != QStringLiteral("USE_APP_PATH")) {
        return QDir::fromNativeSeparators(env);
    }
    return QStringLiteral("C:/Program Files/NVIDIA Corporation/NVIDIA Video Effects");
}
#endif

static bool probeMaxineLibrary()
{
#if defined(PVJ_MAXINE_SDK_LINKED)
    static const char* kProbeEffects[] = {
        NVVFX_FX_ARTIFACT_REDUCTION,
        NVVFX_FX_SUPER_RES,
        NVVFX_FX_SR_UPSCALE,
        NVVFX_FX_DENOISING,
    };
    for (const char* selector : kProbeEffects) {
        NvVFX_Handle probe = nullptr;
        const NvCV_Status probeSt = NvVFX_CreateEffect(selector, &probe);
        if (probeSt != NVCV_SUCCESS || !probe) {
            qWarning() << "Maxine VFX runtime probe failed for" << selector << ":" << probeSt
                       << "(install NVIDIA Video Effects + models; see README Maxine section)";
            return false;
        }
        NvVFX_DestroyEffect(probe);
    }
    return true;
#else
    return false;
#endif
}

struct MaxineFilterBackend::Impl {
    bool sdkReady = false;
    CUstream cudaStream = nullptr;
    EffectSlot effectSlots[4];
};

namespace {

QString g_modelDirectory;

double nodeParam(const pvj::core::CellFilterNode& node, const QString& name, double def)
{
    for (const auto& p : node.params) {
        if (p.name == name) {
            return p.value;
        }
    }
    const auto defaults = pvj::core::defaultParamsFor(node.typeId);
    for (const auto& p : defaults) {
        if (p.name == name) {
            return p.value;
        }
    }
    return def;
}

void copyRgbaToBgrU8(const QImage& rgba, uchar* dstBgr, int dstStride)
{
    const int w = rgba.width();
    const int h = rgba.height();
    for (int y = 0; y < h; ++y) {
        const uchar* src = rgba.constScanLine(y);
        uchar* dst = dstBgr + y * dstStride;
        for (int x = 0; x < w; ++x) {
            dst[x * 3 + 0] = src[x * 4 + 2];
            dst[x * 3 + 1] = src[x * 4 + 1];
            dst[x * 3 + 2] = src[x * 4 + 0];
        }
    }
}

QImage bgrU8ToRgba(const uchar* srcBgr, int w, int h, int srcStride)
{
    QImage rgba(w, h, QImage::Format_RGBA8888);
    for (int y = 0; y < h; ++y) {
        const uchar* src = srcBgr + y * srcStride;
        uchar* dst = rgba.scanLine(y);
        for (int x = 0; x < w; ++x) {
            dst[x * 4 + 0] = src[x * 3 + 2];
            dst[x * 4 + 1] = src[x * 3 + 1];
            dst[x * 4 + 2] = src[x * 3 + 0];
            dst[x * 4 + 3] = 255;
        }
    }
    return rgba;
}

enum class MaxineEffectKind {
    ArtifactReduction = 0,
    SuperResolution = 1,
    Upscale = 2,
    Denoising = 3,
};

MaxineEffectKind effectKindForTypeId(const QString& typeId)
{
    const auto meta = pvj::core::filterEffectMeta(typeId);
    switch (meta.familyId) {
    case 1:
        return MaxineEffectKind::SuperResolution;
    case 2:
        return MaxineEffectKind::Upscale;
    case 3:
        return MaxineEffectKind::Denoising;
    case 0:
    default:
        return MaxineEffectKind::ArtifactReduction;
    }
}

static bool ensureDenoiseState(EffectSlot& slot)
{
    if (!slot.handle) {
        return false;
    }
    if (!slot.denoiseState) {
        NvVFX_StateObjectHandle state = nullptr;
        if (NvVFX_AllocateState(slot.handle, &state) != NVCV_SUCCESS) {
            return false;
        }
        slot.denoiseState = state;
    }
    return NvVFX_SetStateObjectHandleArray(slot.handle, NVVFX_STATE, &slot.denoiseState) == NVCV_SUCCESS;
}

static bool loadEffectAfterImages(EffectSlot& slot, MaxineEffectKind kind)
{
    if (slot.loaded) {
        return true;
    }
    if (kind == MaxineEffectKind::Denoising && !ensureDenoiseState(slot)) {
        qWarning() << "Maxine: denoise state setup failed";
        return false;
    }
    const NvCV_Status loadSt = NvVFX_Load(slot.handle);
    if (loadSt != NVCV_SUCCESS) {
        qWarning() << "NvVFX_Load failed:" << loadSt;
        return false;
    }
    slot.loaded = true;
    return true;
}

const char* effectSelector(MaxineEffectKind kind)
{
    switch (kind) {
    case MaxineEffectKind::SuperResolution:
        return NVVFX_FX_SUPER_RES;
    case MaxineEffectKind::Upscale:
        return NVVFX_FX_SR_UPSCALE;
    case MaxineEffectKind::Denoising:
        return NVVFX_FX_DENOISING;
    case MaxineEffectKind::ArtifactReduction:
    default:
        return NVVFX_FX_ARTIFACT_REDUCTION;
    }
}

bool runPlanarBgrEffect(EffectSlot& slot, MaxineEffectKind kind, CUstream stream, const QImage& rgbaInput,
                        int inW, int inH, int outW, int outH, QImage& rgbaOutput)
{
    if (!ensurePlanarGpuBuffers(slot, inW, inH, outW, outH)) {
        return false;
    }

    copyRgbaToBgrU8(rgbaInput, static_cast<uchar*>(slot.srcCpu.pixels), slot.srcCpu.pitch);
    const NvCV_Status t1 = NvCVImage_Transfer(&slot.srcCpu, &slot.srcGpu, 1.0f / 255.0f, stream, &slot.tmpGpu);
    if (t1 != NVCV_SUCCESS) {
        return false;
    }

    NvVFX_SetImage(slot.handle, NVVFX_INPUT_IMAGE, &slot.srcGpu);
    NvVFX_SetImage(slot.handle, NVVFX_OUTPUT_IMAGE, &slot.dstGpu);
    if (kind == MaxineEffectKind::Denoising && !ensureDenoiseState(slot)) {
        return false;
    }
    if (!loadEffectAfterImages(slot, kind)) {
        return false;
    }
    const NvCV_Status runSt = NvVFX_Run(slot.handle, 0);
    if (runSt != NVCV_SUCCESS) {
        const char* errStr = NvCV_GetErrorStringFromCode(runSt);
        qWarning() << "NvVFX_Run planar failed:" << runSt << (errStr ? errStr : "");
        if (kind == MaxineEffectKind::Denoising) {
            invalidateEffectSlot(slot);
        } else {
            resetEffectSlot(slot);
        }
        return false;
    }

    const NvCV_Status t3 = NvCVImage_Transfer(&slot.dstGpu, &slot.dstCpu, 255.0f, stream, &slot.tmpGpu);
    if (t3 != NVCV_SUCCESS) {
        return false;
    }
    syncCudaDevice();

    rgbaOutput = bgrU8ToRgba(static_cast<const uchar*>(slot.dstCpu.pixels), outW, outH, slot.dstCpu.pitch);
    return true;
}

bool runUpscaleEffect(EffectSlot& slot, MaxineEffectKind kind, CUstream stream, const QImage& rgbaInput,
                      int inW, int inH, int outW, int outH, QImage& rgbaOutput)
{
    if (!checkScaleIsotropy(inW, inH, outW, outH)) {
        qWarning() << "Maxine upscale: non-isotropic scale not supported";
        return false;
    }
    if (!ensureUpscaleGpuBuffers(slot, inW, inH, outW, outH)) {
        return false;
    }

    QImage rgba = rgbaInput;
    if (rgba.format() != QImage::Format_RGBA8888) {
        rgba = rgbaInput.convertToFormat(QImage::Format_RGBA8888);
    }
    NvCVImage packedIn{};
    NvCVImage_Init(&packedIn, inW, inH, rgba.bytesPerLine(), rgba.bits(), NVCV_RGBA, NVCV_U8, NVCV_CHUNKY,
                   NVCV_CPU);
    if (NvCVImage_Transfer(&packedIn, &slot.srcCpu, 1.0f, stream, &slot.tmpGpu) != NVCV_SUCCESS
        || NvCVImage_Transfer(&slot.srcCpu, &slot.srcGpu, 1.0f, stream, &slot.tmpGpu) != NVCV_SUCCESS) {
        return false;
    }

    NvVFX_SetImage(slot.handle, NVVFX_INPUT_IMAGE, &slot.srcGpu);
    NvVFX_SetImage(slot.handle, NVVFX_OUTPUT_IMAGE, &slot.dstGpu);
    if (!loadEffectAfterImages(slot, kind)) {
        return false;
    }
    const NvCV_Status runSt = NvVFX_Run(slot.handle, 0);
    if (runSt != NVCV_SUCCESS) {
        qWarning() << "NvVFX_Run upscale failed:" << runSt;
        resetEffectSlot(slot);
        return false;
    }

    rgbaOutput = QImage(outW, outH, QImage::Format_RGBA8888);
    NvCVImage packedOut{};
    NvCVImage_Init(&packedOut, outW, outH, rgbaOutput.bytesPerLine(), rgbaOutput.bits(), NVCV_RGBA, NVCV_U8,
                   NVCV_CHUNKY, NVCV_CPU);
    if (NvCVImage_Transfer(&slot.dstGpu, &slot.dstCpu, 1.0f, stream, &slot.tmpGpu) != NVCV_SUCCESS
        || NvCVImage_Transfer(&slot.dstCpu, &packedOut, 1.0f, stream, &slot.tmpGpu) != NVCV_SUCCESS) {
        return false;
    }
    syncCudaDevice();
    return true;
}

} // namespace

MaxineFilterBackend::~MaxineFilterBackend()
{
    shutdown();
}

bool maxineFiltersCompiled()
{
    return true;
}

bool maxineFiltersAvailable()
{
    return g_maxineRuntimeAvailable;
}

void initializeMaxineRuntime()
{
    if (g_maxineRuntimeProbed) {
        return;
    }
    g_maxineRuntimeProbed = true;

#if defined(Q_OS_WIN) && defined(PVJ_MAXINE_SDK_LINKED)
    const QString runtimeDir = defaultMaxineRuntimeDirectory();
    const QString dllPath = runtimeDir + QStringLiteral("/NVVideoEffects.dll");
    if (!QFileInfo::exists(dllPath)) {
        qWarning() << "Maxine: NVVideoEffects.dll not found at" << dllPath;
        g_maxineRuntimeAvailable = false;
        return;
    }

    g_nvVfxDllDirectoryStorage = QDir::toNativeSeparators(runtimeDir).toUtf8();
    g_nvVFXSDKPath = g_nvVfxDllDirectoryStorage.data();
    SetDllDirectoryW(reinterpret_cast<const wchar_t*>(runtimeDir.utf16()));

    const QString modelDir = runtimeDir + QStringLiteral("/models");
    if (QFileInfo::exists(modelDir)) {
        setMaxineModelDirectory(modelDir);
    }

    if (initCudaDevice()) {
        qInfo() << "Maxine: CUDA device 0 selected";
    } else {
        qWarning() << "Maxine: cudaSetDevice(0) unavailable (cudart64_12.dll next to exe?)";
    }

    const int cc = detectComputeCapability();
    g_maxineRuntimeAvailable = probeMaxineLibrary();
    if (g_maxineRuntimeAvailable) {
        qInfo() << "Maxine VFX runtime ready:" << runtimeDir << "compute_cap=" << cc;
    }
#else
    g_maxineRuntimeAvailable = probeMaxineLibrary();
#endif
}

void setMaxineModelDirectory(const QString& path)
{
    g_modelDirectory = path;
}

bool MaxineFilterBackend::loadMaxineEffect(int slotIndex)
{
    if (!m_impl || slotIndex < 0 || slotIndex > 3) {
        return false;
    }
    const auto kind = static_cast<MaxineEffectKind>(slotIndex);
    EffectSlot& slot = m_impl->effectSlots[slotIndex];
    if (slot.handle) {
        return true;
    }
    const NvCV_Status st = NvVFX_CreateEffect(effectSelector(kind), &slot.handle);
    if (st != NVCV_SUCCESS || !slot.handle) {
        qWarning() << "NvVFX_CreateEffect failed:" << st;
        return false;
    }
    if (!g_modelDirectory.isEmpty() && kind != MaxineEffectKind::Upscale) {
        const QByteArray dir = QDir::toNativeSeparators(g_modelDirectory).toUtf8();
        NvVFX_SetString(slot.handle, NVVFX_MODEL_DIRECTORY, dir.constData());
    }
    if (m_impl->cudaStream) {
        NvVFX_SetCudaStream(slot.handle, NVVFX_CUDA_STREAM, m_impl->cudaStream);
    }
    return true;
}

bool MaxineFilterBackend::ensureInitialized(QRhi* /*rhi*/)
{
    if (!m_impl) {
        m_impl = new Impl();
    }
    if (m_impl->sdkReady) {
        return g_maxineRuntimeAvailable;
    }

    m_impl->sdkReady = true;

    if (!g_maxineRuntimeProbed) {
        initializeMaxineRuntime();
    } else if (!g_maxineRuntimeAvailable) {
        return false;
    }

    if (g_maxineRuntimeAvailable && !m_impl->cudaStream) {
        CUstream stream = nullptr;
        const NvCV_Status streamSt = NvVFX_CudaStreamCreate(&stream);
        if (streamSt == NVCV_SUCCESS) {
            m_impl->cudaStream = stream;
        }
    }

    return g_maxineRuntimeAvailable;
}

void MaxineFilterBackend::shutdown()
{
    if (!m_impl) {
        g_maxineRuntimeAvailable = false;
        return;
    }
    for (auto& slot : m_impl->effectSlots) {
        resetEffectSlot(slot);
    }
    if (m_impl->cudaStream) {
        NvVFX_CudaStreamDestroy(m_impl->cudaStream);
        m_impl->cudaStream = nullptr;
    }
    delete m_impl;
    m_impl = nullptr;
    g_maxineRuntimeAvailable = false;
}

void MaxineFilterBackend::invalidateSize()
{
    if (!m_impl) {
        return;
    }
    for (auto& slot : m_impl->effectSlots) {
        resetEffectSlot(slot);
    }
}

bool MaxineFilterBackend::apply(QRhi* rhi, QRhiCommandBuffer* cb, const pvj::core::CellFilterNode& node,
                                QRhiTexture* sourceTex, QRhiTextureRenderTarget* targetRt,
                                const QSize& stagePx)
{
    if (!m_impl || !rhi || !cb || !sourceTex || !targetRt || stagePx.isEmpty()) {
        return false;
    }
    QImage input;
    if (!readRgba8Texture(rhi, cb, sourceTex, stagePx, input)) {
        qWarning() << "Maxine: texture readback failed";
        return false;
    }
    return applyFromImage(rhi, cb, node, input, targetRt, stagePx);
}

bool MaxineFilterBackend::applyFromImage(QRhi* rhi, QRhiCommandBuffer* cb,
                                         const pvj::core::CellFilterNode& node, const QImage& input,
                                         QRhiTextureRenderTarget* targetRt, const QSize& stagePx)
{
    if (!m_impl || !rhi || !cb || input.isNull() || !targetRt || stagePx.isEmpty()) {
        return false;
    }
    if (!pvj::core::filterUsesMaxineBackend(node.typeId)) {
        return false;
    }
    if (!ensureInitialized(rhi)) {
        return false;
    }

    const MaxineEffectKind kind = effectKindForTypeId(node.typeId);
    const int slotIndex = static_cast<int>(kind);
    if (!loadMaxineEffect(slotIndex)) {
        return false;
    }
    EffectSlot& slot = m_impl->effectSlots[slotIndex];

    const int inW = input.width();
    const int inH = input.height();
    QImage processInput = input;
    int procW = inW;
    int procH = inH;
    double outScale = 1.0;
    if (kind == MaxineEffectKind::SuperResolution) {
        outScale = qBound(1.0, nodeParam(node, QStringLiteral("scale"), 1.5), 2.0);
    } else if (kind == MaxineEffectKind::Upscale) {
        outScale = qBound(1.0, nodeParam(node, QStringLiteral("scale"), 2.0), 4.0);
    }
    const bool sameSizeOut = kind == MaxineEffectKind::ArtifactReduction
        || kind == MaxineEffectKind::Denoising;
    const int outW = sameSizeOut ? procW : qMax(1, int(procW * outScale + 0.5));
    const int outH = sameSizeOut ? procH : qMax(1, int(procH * outScale + 0.5));

    if (slot.lastInput != QSize(procW, procH) || slot.lastOutput != QSize(outW, outH)) {
        slot.loaded = false;
        freeGpuBuffers(slot);
        if (kind == MaxineEffectKind::Denoising) {
            freeDenoiseState(slot);
        }
        slot.lastInput = QSize(procW, procH);
        slot.lastOutput = QSize(outW, outH);
    }

    if (m_impl->cudaStream) {
        NvVFX_SetCudaStream(slot.handle, NVVFX_CUDA_STREAM, m_impl->cudaStream);
    }

    const int mode = int(nodeParam(node, QStringLiteral("strength"), 1.0) + 0.5) & 1;
    if (kind == MaxineEffectKind::ArtifactReduction) {
        NvVFX_SetU32(slot.handle, NVVFX_MODE, static_cast<unsigned>(mode));
    } else if (kind == MaxineEffectKind::SuperResolution) {
        if (!checkScaleIsotropy(procW, procH, outW, outH)) {
            qWarning() << "Maxine super resolution: non-isotropic scale not supported";
            return false;
        }
        NvVFX_SetU32(slot.handle, NVVFX_MODE, static_cast<unsigned>(mode));
        NvVFX_SetF32(slot.handle, NVVFX_STRENGTH, 0.0f);
    } else if (kind == MaxineEffectKind::Denoising) {
        // Weak/Strong map to non-zero strengths; NVVFX_STRENGTH=0 breaks TensorRT denoise.
        const float denoiseStrength = (mode == 0) ? 0.35f : 1.0f;
        NvVFX_SetF32(slot.handle, NVVFX_STRENGTH, denoiseStrength);
    } else if (kind == MaxineEffectKind::Upscale) {
        if (!checkScaleIsotropy(procW, procH, outW, outH)) {
            qWarning() << "Maxine upscale: non-isotropic scale not supported";
            return false;
        }
        const float strength = float(qBound(0.0, nodeParam(node, QStringLiteral("amount"), 0.5), 1.0));
        NvVFX_SetF32(slot.handle, NVVFX_STRENGTH, strength);
    }

    QImage output;
    if (kind == MaxineEffectKind::Upscale) {
        if (!runUpscaleEffect(slot, kind, m_impl->cudaStream, processInput, procW, procH, outW, outH, output)) {
            return false;
        }
    } else if (!runPlanarBgrEffect(slot, kind, m_impl->cudaStream, processInput, procW, procH, outW, outH, output)) {
        static bool s_logged = false;
        if (!s_logged) {
            qWarning() << "Maxine: effect run failed for" << node.typeId;
            s_logged = true;
        }
        return false;
    }

    if (processInput.size() != input.size()) {
        output = output.scaled(input.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    const double mix = qBound(0.0,
                              nodeParam(node, QStringLiteral("blend"),
                                        nodeParam(node, QStringLiteral("amount"), 1.0)),
                              1.0);
    if (mix < 1.0 - 1e-6) {
        QImage blended = input;
        if (output.size() != input.size()) {
            output = output.scaled(input.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        for (int y = 0; y < blended.height(); ++y) {
            uchar* dst = blended.scanLine(y);
            const uchar* src = output.constScanLine(y);
            for (int x = 0; x < blended.width(); ++x) {
                const int o = x * 4;
                for (int c = 0; c < 3; ++c) {
                    dst[o + c] = uchar(dst[o + c] * (1.0 - mix) + src[o + c] * mix);
                }
            }
        }
        output = blended;
    } else if (output.size() != stagePx) {
        output = output.scaled(stagePx, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    if (!uploadRgba8ToRenderTarget(rhi, cb, output, targetRt, stagePx)) {
        return false;
    }
    return true;
}

} // namespace pvj::render

#else // !PVJ_MAXINE_SDK_LINKED

namespace pvj::render {

// Fallback when this translation unit is built without SDK (should use stub instead).
bool maxineFiltersCompiled() { return false; }
bool maxineFiltersAvailable() { return false; }
void initializeMaxineRuntime() {}
void setMaxineModelDirectory(const QString&) {}
struct MaxineFilterBackend::Impl {};
MaxineFilterBackend::~MaxineFilterBackend() = default;
bool MaxineFilterBackend::ensureInitialized(QRhi*) { return false; }
void MaxineFilterBackend::shutdown() {}
void MaxineFilterBackend::invalidateSize() {}
bool MaxineFilterBackend::apply(QRhi*, QRhiCommandBuffer*, const pvj::core::CellFilterNode&, QRhiTexture*,
                                QRhiTextureRenderTarget*, const QSize&)
{
    return false;
}
bool MaxineFilterBackend::applyFromImage(QRhi*, QRhiCommandBuffer*, const pvj::core::CellFilterNode&,
                                         const QImage&, QRhiTextureRenderTarget*, const QSize&)
{
    return false;
}

} // namespace pvj::render

#endif
