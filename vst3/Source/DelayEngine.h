#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace afterglow
{
struct DelaySettings
{
    bool enabled = false, sync = false;
    float timeMs = 320.0f, feedback = 0.35f, toneHz = 4000.0f;
    float drive = 0.25f, pingPong = 0.0f, mix = 1.0f;
    int division = 3, timing = 0;
    double bpm = 120.0;
};

inline double delaySeconds(const DelaySettings& s) noexcept
{
    constexpr std::array<double, 7> beats { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 0.0625 };
    const auto tempo = std::isfinite(s.bpm) && s.bpm > 0.0 ? s.bpm : 120.0;
    const auto style = s.timing == 1 ? 1.5 : s.timing == 2 ? 2.0 / 3.0 : 1.0;
    const auto seconds = s.sync
        ? 60.0 / tempo * beats[static_cast<size_t>(std::clamp(s.division, 0, 6))] * style
        : static_cast<double>(s.timeMs) / 1000.0;
    return std::clamp(std::isfinite(seconds) ? seconds : 0.320, 0.020, 24.0);
}

class DelayEngine
{
public:
    void prepare(double sampleRate, const DelaySettings& settings)
    {
        rate = std::isfinite(sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        const auto size = static_cast<size_t>(std::ceil(rate * 24.0)) + 8;
        for (auto& channel : ring) channel.assign(size, 0.0f);
        write = 0;
        low1.fill(0); low2.fill(0); previousInput.fill(0); highState.fill(0);
        delayA = delayB = requested = delaySeconds(settings) * rate;
        fadeLength = std::max(1, static_cast<int>(rate * 0.040));
        fadePosition = fadeLength;
        smoothing = static_cast<float>(1.0 - std::exp(-1.0 / (rate * 0.020)));
        highCoefficient = static_cast<float>(std::exp(-2.0 * pi * 50.0 / rate));
        active = settings.enabled ? 1.0f : 0.0f;
        feedback = safe(settings.feedback, 0.0f, 0.95f);
        mix = safe(settings.mix, 0.0f, 1.0f);
        drive = safe(settings.drive, 0.0f, 1.0f) * 4.0f;
        pingPong = safe(settings.pingPong, 0.0f, 1.0f);
        lowCoefficient = toneCoefficient(settings.toneHz);
    }

    void process(float* const* channels, int channelCount, int samples,
                 const DelaySettings& settings) noexcept
    {
        if (ring[0].empty() || channelCount < 1 || samples < 1) return;
        requested = delaySeconds(settings) * rate;
        const float targetActive = settings.enabled ? 1.0f : 0.0f;
        const float targetFeedback = safe(settings.feedback, 0.0f, 0.95f);
        const float targetMix = safe(settings.mix, 0.0f, 1.0f);
        const float targetDrive = safe(settings.drive, 0.0f, 1.0f) * 4.0f;
        const float targetPingPong = safe(settings.pingPong, 0.0f, 1.0f);
        const float targetLow = toneCoefficient(settings.toneHz);
        for (int i = 0; i < samples; ++i)
        {
            approach(active, targetActive); approach(feedback, targetFeedback);
            approach(mix, targetMix); approach(drive, targetDrive);
            approach(pingPong, targetPingPong); approach(lowCoefficient, targetLow);
            if (active * mix < 1.0e-6f)
            {
                delayA = delayB = requested;
                fadePosition = fadeLength;
            }
            else if (fadePosition == fadeLength && std::abs(requested - delayA) > 0.25)
            {
                delayB = requested;
                fadePosition = 0;
            }
            const float blend = static_cast<float>(fadePosition) / static_cast<float>(fadeLength);
            std::array<float, 2> tap {}, loop {};
            for (size_t ch = 0; ch < 2; ++ch)
            {
                const float first = read(ch, delayA);
                tap[ch] = fadePosition == fadeLength ? first
                    : first + (read(ch, delayB) - first) * blend;
                const float hp = highCoefficient * (highState[ch] + tap[ch] - previousInput[ch]);
                previousInput[ch] = tap[ch]; highState[ch] = hp;
                low1[ch] += lowCoefficient * (hp - low1[ch]);
                low2[ch] += lowCoefficient * (low1[ch] - low2[ch]);
                const float coloured = drive < 0.0001f ? low2[ch]
                    : std::tanh(drive * low2[ch]) / drive;
                loop[ch] = coloured * feedback;
            }
            const float left = channels[0][i];
            const float right = channelCount > 1 ? channels[1][i] : left;
            const float p = channelCount > 1 ? pingPong : 0.0f;
            const float sendLeft = left * (1.0f - p) + (left + right) * 0.5f * p;
            ring[0][write] = sendLeft * active + loop[0] * (1.0f - p) + loop[1] * p;
            ring[1][write] = right * active * (1.0f - p) + loop[1] * (1.0f - p) + loop[0] * p;
            channels[0][i] = left + tap[0] * 0.5f * active * mix;
            if (channelCount > 1) channels[1][i] = right + tap[1] * 0.5f * active * mix;
            write = (write + 1) % ring[0].size();
            if (fadePosition < fadeLength && ++fadePosition == fadeLength) delayA = delayB;
        }
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    static float safe(float value, float min, float max) noexcept
    {
        return std::isfinite(value) ? std::clamp(value, min, max) : min;
    }
    void approach(float& value, float target) const noexcept
    {
        value += smoothing * (target - value);
        if (std::abs(value - target) < 1.0e-7f) value = target;
    }
    float toneCoefficient(float frequency) const noexcept
    {
        // Two dampers in the feedback loop. Calibrate the combined corner approximately.
        const double f = std::min(rate * 0.45, static_cast<double>(safe(frequency, 500, 14000)) * 1.553774);
        return static_cast<float>(1.0 - std::exp(-2.0 * pi * f / rate));
    }
    float read(size_t ch, double delay) const noexcept
    {
        double position = static_cast<double>(write) - delay;
        if (position < 0.0) position += static_cast<double>(ring[ch].size());
        const auto index = static_cast<size_t>(position);
        const float x = static_cast<float>(position - static_cast<double>(index));
        const auto size = ring[ch].size();
        const float a = ring[ch][(index + size - 1) % size], b = ring[ch][index % size];
        const float c = ring[ch][(index + 1) % size], d = ring[ch][(index + 2) % size];
        return b + 0.5f * x * (c - a + x * (2.0f * a - 5.0f * b + 4.0f * c - d
                         + x * (3.0f * (b - c) + d - a)));
    }
    std::array<std::vector<float>, 2> ring;
    std::array<float, 2> low1 {}, low2 {}, previousInput {}, highState {};
    size_t write = 0;
    double rate = 48000.0, delayA = 15360, delayB = 15360, requested = 15360;
    float smoothing = 0.001f, highCoefficient = 0.99f, lowCoefficient = 0.5f;
    float active = 0, feedback = 0, mix = 1, drive = 0, pingPong = 0;
    int fadeLength = 1920, fadePosition = 1920;
};
}
