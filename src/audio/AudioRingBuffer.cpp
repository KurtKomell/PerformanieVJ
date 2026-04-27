#include "AudioRingBuffer.h"

#include <algorithm>

namespace pvj::audio {

void AudioRingBuffer::setCapacityFrames(std::size_t maxFrames)
{
    QMutexLocker lock(&m_mutex);
    m_capacityFrames = std::max<std::size_t>(1, maxFrames);
    m_data.assign(m_capacityFrames * 2, 0.f);
    m_readIndex    = 0;
    m_filledFrames = 0;
}

void AudioRingBuffer::clear()
{
    QMutexLocker lock(&m_mutex);
    m_filledFrames = 0;
    m_readIndex    = 0;
}

void AudioRingBuffer::writeFrames(const float* interleavedStereo, std::size_t frameCount)
{
    if (frameCount == 0 || m_capacityFrames == 0) return;

    QMutexLocker lock(&m_mutex);

    // Drop oldest samples if needed.
    while (m_filledFrames + frameCount > m_capacityFrames) {
        m_readIndex = (m_readIndex + 1) % m_capacityFrames;
        --m_filledFrames;
    }

    for (std::size_t f = 0; f < frameCount; ++f) {
        const std::size_t wi = ((m_readIndex + m_filledFrames) % m_capacityFrames) * 2;
        m_data[wi + 0] = interleavedStereo[f * 2 + 0];
        m_data[wi + 1] = interleavedStereo[f * 2 + 1];
        ++m_filledFrames;
    }
}

void AudioRingBuffer::readMixAdd(float* out, std::size_t frameCount, float gain)
{
    if (frameCount == 0 || gain == 0.f || m_capacityFrames == 0) return;

    QMutexLocker lock(&m_mutex);

    const std::size_t take = std::min(frameCount, m_filledFrames);
    for (std::size_t f = 0; f < take; ++f) {
        const std::size_t ri = (m_readIndex % m_capacityFrames) * 2;
        out[f * 2 + 0] += gain * m_data[ri + 0];
        out[f * 2 + 1] += gain * m_data[ri + 1];
        m_readIndex = (m_readIndex + 1) % m_capacityFrames;
        --m_filledFrames;
    }
}

std::size_t AudioRingBuffer::availableFrames() const
{
    QMutexLocker lock(&m_mutex);
    return m_filledFrames;
}

} // namespace pvj::audio
