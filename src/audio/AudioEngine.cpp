#include "AudioEngine.h"
#include "AudioRingBuffer.h"

#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

namespace pvj::audio {
namespace detail {

class AudioEngineImpl
{
public:
    std::array<AudioRingBuffer, AudioEngine::kMaxLayers> ringBuffers {};
    std::array<float, AudioEngine::kMaxLayers>             layerGain {};
    std::array<bool, AudioEngine::kMaxLayers>              layerActive {};
    float                                                  masterGain = 1.f;

    ma_device        device {};
    ma_device_config config {};
    bool             deviceInitialized = false;

    void mixCallback(float* out, ma_uint32 frameCount)
    {
        const std::size_t n = static_cast<std::size_t>(frameCount) * 2;
        std::memset(out, 0, n * sizeof(float));

        for (int i = 0; i < AudioEngine::kMaxLayers; ++i) {
            if (!layerActive[static_cast<std::size_t>(i)]) continue;
            const float g = layerGain[static_cast<std::size_t>(i)];
            if (g <= 0.f) continue;
            ringBuffers[static_cast<std::size_t>(i)].readMixAdd(out, frameCount, g);
        }

        const float m = masterGain;
        for (std::size_t j = 0; j < n; ++j) {
            float s = out[j] * m;
            s = std::clamp(s, -1.f, 1.f);
            out[j] = s;
        }
    }
};

} // namespace detail
} // namespace pvj::audio

namespace pvj::audio {

namespace {

void dataCallback(ma_device* pDevice, void* pOutput, const void* /*pInput*/,
                  ma_uint32 frameCount)
{
    auto* impl = static_cast<detail::AudioEngineImpl*>(pDevice->pUserData);
    impl->mixCallback(static_cast<float*>(pOutput), frameCount);
}

} // namespace

AudioEngine::AudioEngine(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<detail::AudioEngineImpl>())
{}

AudioEngine::~AudioEngine()
{
    shutdown();
}

bool AudioEngine::initialize(int sampleRate)
{
    shutdown();

    const int sr = (sampleRate > 0) ? sampleRate : kDefaultSampleRate;
    for (auto& rb : m_impl->ringBuffers) {
        rb.setCapacityFrames(static_cast<std::size_t>(sr));
    }

    m_impl->config = ma_device_config_init(ma_device_type_playback);
    m_impl->config.playback.format   = ma_format_f32;
    m_impl->config.playback.channels = 2;
    m_impl->config.sampleRate        = static_cast<ma_uint32>(sr);
    m_impl->config.dataCallback      = dataCallback;
    m_impl->config.pUserData         = m_impl.get();

    if (ma_device_init(nullptr, &m_impl->config, &m_impl->device) != MA_SUCCESS) {
        return false;
    }
    m_impl->deviceInitialized = true;

    if (ma_device_start(&m_impl->device) != MA_SUCCESS) {
        ma_device_uninit(&m_impl->device);
        m_impl->deviceInitialized = false;
        return false;
    }

    m_running = true;
    return true;
}

void AudioEngine::shutdown()
{
    if (!m_impl) return;

    if (m_impl->deviceInitialized) {
        ma_device_uninit(&m_impl->device);
        m_impl->deviceInitialized = false;
    }

    for (auto& rb : m_impl->ringBuffers) {
        rb.clear();
    }
    m_running = false;
}

AudioRingBuffer& AudioEngine::layerBuffer(int layer)
{
    Q_ASSERT(layer >= 0 && layer < kMaxLayers);
    return m_impl->ringBuffers[static_cast<std::size_t>(layer)];
}

void AudioEngine::setLayerGain(int layer, float gain)
{
    if (layer < 0 || layer >= kMaxLayers) return;
    m_impl->layerGain[static_cast<std::size_t>(layer)] =
        std::clamp(gain, 0.f, 1.f);
}

void AudioEngine::setLayerActive(int layer, bool active)
{
    if (layer < 0 || layer >= kMaxLayers) return;
    m_impl->layerActive[static_cast<std::size_t>(layer)] = active;
}

void AudioEngine::setMasterGain(float gain)
{
    m_impl->masterGain = std::clamp(gain, 0.f, 4.f);
}

} // namespace pvj::audio
