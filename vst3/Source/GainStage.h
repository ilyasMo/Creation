#pragma once
#include <algorithm>
#include <cmath>

namespace afterglow
{
// No allocation, locks, file access, or network work on the audio thread.
class GainStage
{
public:
    void prepare(double sampleRate, float initialGain = 1.0f) noexcept
    {
        rampLength = std::max(1, static_cast<int>(sampleRate * 0.020));
        current = target = initialGain;
        remaining = 0;
        step = 0.0f;
    }

    void process(float* const* channels, int channelCount, int samples,
                 float requestedGain) noexcept
    {
        const float safeGain = std::isfinite(requestedGain)
                                 ? std::clamp(requestedGain, 0.0f, 4.0f) : 1.0f;
        if (safeGain != target)
        {
            target = safeGain;
            remaining = rampLength;
            step = (target - current) / static_cast<float>(rampLength);
        }
        for (int sample = 0; sample < samples; ++sample)
        {
            if (remaining > 0)
            {
                current += step;
                if (--remaining == 0)
                    current = target;
            }
            for (int channel = 0; channel < channelCount; ++channel)
                channels[channel][sample] *= current;
        }
    }

private:
    float current = 1.0f, target = 1.0f, step = 0.0f;
    int remaining = 0, rampLength = 960;
};
}
