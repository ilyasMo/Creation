#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include "FeedbackColour.h"

namespace afterglow
{
struct DelaySettings
{
    bool enabled = false, sync = false, eqEnabled = false, tapeEnabled = false;
    float timeMs = 320.0f, feedback = 0.35f, lowCutHz = 20.0f, highCutHz = 20000.0f;
    float wow = 0.0f, flutter = 0.0f, pingPong = 0.0f, mix = 1.0f;
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
    return std::clamp(std::isfinite(seconds) ? seconds : 0.320, s.sync ? 0.001 : 0.020, 24.0);
}

class DelayEngine
{
public:
    void prepare(double sampleRate, const DelaySettings& settings)
    {
        rate = std::isfinite(sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        const auto size = static_cast<size_t>(std::ceil(rate * 24.02)) + 8;
        for (auto& channel : ring) channel.assign(size, 0.0f);
        write = 0;
        for (auto& filter : lowCutFilters) filter.reset();
        for (auto& filter : highCutFilters) filter.reset();
        phases.fill(0.0);
        delayA = delayB = requested = delaySeconds(settings) * rate;
        fadeLength = std::max(1, static_cast<int>(rate * 0.040));
        fadePosition = fadeLength;
        smoothing = static_cast<float>(1.0 - std::exp(-1.0 / (rate * 0.020)));
        active = settings.enabled ? 1.0f : 0.0f;
        eqActive = settings.eqEnabled ? 1.0f : 0.0f;
        feedback = safe(settings.feedback, 0.0f, 0.95f);
        mix = safe(settings.mix, 0.0f, 1.0f);
        wow = settings.tapeEnabled ? safe(settings.wow, 0.0f, 1.0f) : 0.0f;
        flutter = settings.tapeEnabled ? safe(settings.flutter, 0.0f, 1.0f) : 0.0f;
        pingPong = safe(settings.pingPong, 0.0f, 1.0f);
        lowCutG = filterCoefficient(settings.lowCutHz);
        highCutG = filterCoefficient(settings.highCutHz);
    }

    void process(float* const* channels, int channelCount, int samples,
                 const DelaySettings& settings) noexcept
    {
        if (ring[0].empty() || channelCount < 1 || samples < 1) return;
        requested = delaySeconds(settings) * rate;
        const float targetActive = settings.enabled ? 1.0f : 0.0f;
        const float targetEq = settings.eqEnabled ? 1.0f : 0.0f;
        const float targetFeedback = safe(settings.feedback, 0.0f, 0.95f);
        const float targetMix = safe(settings.mix, 0.0f, 1.0f);
        const float targetWow = settings.tapeEnabled ? safe(settings.wow, 0.0f, 1.0f) : 0.0f;
        const float targetFlutter = settings.tapeEnabled ? safe(settings.flutter, 0.0f, 1.0f) : 0.0f;
        const float targetPingPong = safe(settings.pingPong, 0.0f, 1.0f);
        const float targetLowCut = filterCoefficient(settings.lowCutHz);
        const float targetHighCut = filterCoefficient(settings.highCutHz);
        for (int i = 0; i < samples; ++i)
        {
            approach(active, targetActive); approach(eqActive, targetEq); approach(feedback, targetFeedback);
            approach(mix, targetMix); approach(wow, targetWow); approach(flutter, targetFlutter);
            approach(pingPong, targetPingPong);
            approach(lowCutG, targetLowCut); approach(highCutG, targetHighCut);
            const double modulation = tapeModulation();
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
                const float first = read(ch, delayA + modulation);
                tap[ch] = fadePosition == fadeLength ? first
                    : first + (read(ch, delayB + modulation) - first) * blend;
                loop[ch] = tap[ch] * feedback;
            }
            const float left = channels[0][i];
            const float right = channelCount > 1 ? channels[1][i] : left;
            const float p = channelCount > 1 ? pingPong : 0.0f;
            const float sendLeft = left * (1.0f - p) + (left + right) * 0.5f * p;
            ring[0][write] = sendLeft * active + loop[0] * (1.0f - p) + loop[1] * p;
            ring[1][write] = right * active * (1.0f - p) + loop[1] * (1.0f - p) + loop[0] * p;
            for (int channel = 0; channel < std::min(channelCount, 2); ++channel)
            {
                const auto ch = static_cast<size_t>(channel);
                const float original = channel == 0 ? left : right;
                const float processed = original + tap[ch] * 0.5f * active;
                const float highPassed = lowCutFilters[ch].process(processed, lowCutG, true);
                const float filtered = highCutFilters[ch].process(highPassed, highCutG);
                const float wholeChain = processed + (filtered - processed) * eqActive;
                channels[channel][i] = original + (wholeChain - original) * mix;
            }
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
    float filterCoefficient(float frequency) const noexcept
    {
        return FeedbackFilter::conductance(rate, safe(frequency, 20, 20000));
    }
    double tapeModulation() noexcept
    {
        double modulation = 0.0;
        if (wow > 1.0e-7f)
            modulation += wow * 0.003 * (0.85 * std::sin(phases[0]) + 0.15 * std::sin(phases[1]));
        if (flutter > 1.0e-7f)
            modulation += flutter * 0.0003 * (0.75 * std::sin(phases[2]) + 0.25 * std::sin(phases[3]));
        constexpr std::array<double, 4> speeds { 0.45, 0.17, 7.5, 11.31 };
        for (size_t i = 0; i < phases.size(); ++i)
        {
            phases[i] += 2.0 * pi * speeds[i] / rate;
            if (phases[i] >= 2.0 * pi) phases[i] -= 2.0 * pi;
        }
        return modulation * rate;
    }
    float read(size_t ch, double delay) const noexcept
    {
        delay = std::clamp(delay, 3.0, static_cast<double>(ring[ch].size()) - 4.0);
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
    std::array<FeedbackFilter, 2> lowCutFilters, highCutFilters;
    std::array<double, 4> phases {};
    size_t write = 0;
    double rate = 48000.0, delayA = 15360, delayB = 15360, requested = 15360;
    float smoothing = 0.001f, lowCutG = 0.001f, highCutG = 1.0f;
    float active = 0, eqActive = 0, feedback = 0, mix = 1, wow = 0, flutter = 0, pingPong = 0;
    int fadeLength = 1920, fadePosition = 1920;
};
}
