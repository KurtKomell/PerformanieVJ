#pragma once

#include <QMutex>

#include <cstddef>
#include <vector>

namespace pvj::audio {

// Thread-safe stereo float32 ring buffer (interleaved LRLR…). Writer: FFmpeg
// decode thread. Reader: miniaudio device callback.
class AudioRingBuffer
{
public:
    void setCapacityFrames(std::size_t maxFrames);
    void clear();

    // Drops oldest samples if the buffer would overflow (keeps latency bounded).
    void writeFrames(const float* interleavedStereo, std::size_t frameCount);

    // Adds up to `frameCount` frames into `out` (interleaved stereo), scaled by
    // `gain`. Missing samples are treated as silence. `out` must hold
    // `frameCount * 2` floats.
    void readMixAdd(float* out, std::size_t frameCount, float gain);

    std::size_t availableFrames() const;

private:
    mutable QMutex     m_mutex;
    std::vector<float> m_data; // interleaved stereo, capacity = m_capacityFrames * 2
    std::size_t        m_capacityFrames = 0;
    std::size_t        m_readIndex      = 0; // frame index
    std::size_t        m_filledFrames   = 0;
};

} // namespace pvj::audio
