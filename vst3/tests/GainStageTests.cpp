#include "../Source/GainStage.h"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
int main()
{
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        afterglow::GainStage stage;
        stage.prepare(sampleRate);
        std::array<float, 2048> left {}, right {};
        for (int i = 0; i < 2048; ++i)
        {
            left[i] = std::sin(static_cast<float>(i) * 0.7f) * 0.3f;
            right[i] = static_cast<float>((i % 13) - 6) / 30.0f;
        }
        const auto originalLeft = left, originalRight = right;
        float* channels[] = { left.data(), right.data() };
        stage.process(channels, 2, 2048, 1.0f);
        require(left == originalLeft && right == originalRight, "Unity changes audio");
        left.fill(1.0f); right.fill(1.0f);
        stage.process(channels, 2, 2048, 0.25f);
        require(left.front() > 0.99f, "Gain change clicks at first sample");
        require(std::abs(left.back() - 0.25f) < 1.0e-6f, "Gain does not settle");
        require(left == right, "Stereo channels use different gain ramps");
        for (int i = 1; i < 2048; ++i)
            require(std::abs(left[i] - left[i - 1]) < 0.002f, "Discontinuous gain ramp");
        left.fill(1.0f);
        stage.process(channels, 1, 2048, 1.0f);
        require(std::abs(left.back() - 1.0f) < 1.0e-6f, "Bypass unity does not return");
        left.fill(0.0f);
        stage.process(channels, 1, 2048, std::numeric_limits<float>::quiet_NaN());
        for (float value : left) require(value == 0.0f, "Silence/invalid target is not safe");
        stage.process(channels, 1, 0, 1.0f);
    }
    std::cout << "Passed mono/stereo unity, smooth gain, bypass return, silence and 44.1/48/96 kHz checks.\n";
}
