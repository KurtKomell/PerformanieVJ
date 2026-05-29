#pragma once

#include <memory>

#include <QObject>

#include <cstddef>

namespace pvj::audio {

class AudioRingBuffer;

namespace detail {
class AudioEngineImpl;
}

// Playback device (miniaudio) mixing stereo float32 layers into the default
// output. Each mix layer reads from an `AudioRingBuffer` fed by
// `FfmpegAudioDecoder`.
class AudioEngine : public QObject
{
    Q_OBJECT
public:
    static constexpr int kMaxLayers         = 14;
    static constexpr int kDefaultSampleRate = 48000;

    explicit AudioEngine(QObject* parent = nullptr);
    ~AudioEngine() override;

    bool initialize(int sampleRate = kDefaultSampleRate);
    void shutdown();

    bool isRunning() const { return m_running; }

    AudioRingBuffer& layerBuffer(int layer);
    void setLayerGain(int layer, float gain);
    void setLayerActive(int layer, bool active);
    void setMasterGain(float gain);

private:
    std::unique_ptr<detail::AudioEngineImpl> m_impl;
    bool m_running = false;
};

} // namespace pvj::audio
