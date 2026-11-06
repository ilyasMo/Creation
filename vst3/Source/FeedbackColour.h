#pragma once
#include <algorithm>
#include <cmath>

namespace afterglow
{
// First-order antiderivative antialiasing for the feedback soft saturator.
// Work in double precision to keep divided differences stable at low levels.
class FeedbackSaturator
{
public:
    void reset() noexcept { previous = 0.0; }
    float process(float input, float amount) noexcept
    {
        const double x = input, before = previous;
        previous = x;
        const double k = std::clamp(static_cast<double>(amount), 0.0, 4.0);
        const double midpoint = (x + before) * 0.5;
        if (k < 0.01) return static_cast<float>(midpoint);
        const double difference = x - before;
        if (std::abs(difference) < 0.0001)
            return static_cast<float>(std::tanh(k * midpoint) / k);
        return static_cast<float>((logCosh(k * x) - logCosh(k * before))
                                 / (k * k * difference));
    }
private:
    static double logCosh(double x) noexcept
    {
        const double a = std::abs(x);
        return a + std::log1p(std::exp(-2.0 * a)) - 0.69314718055994530942;
    }
    double previous = 0.0;
};

// Topology-preserving state-variable low-pass, configured as Butterworth (Q=1/sqrt(2)).
// Smoothed integrator conductance keeps the feedback tone useful across sample rates.
class FeedbackLowPass
{
public:
    void reset() noexcept { state1 = state2 = 0.0; }
    static float conductance(double rate, float frequency) noexcept
    {
        const double f = std::clamp(static_cast<double>(frequency), 20.0, rate * 0.45);
        return static_cast<float>(std::tan(3.14159265358979323846 * f / rate));
    }
    float process(float input, float g) noexcept
    {
        const double conductance = g;
        const double a1 = 1.0 / (1.0 + conductance * (conductance + 1.4142135623730950488));
        const double a2 = conductance * a1, a3 = conductance * a2;
        const double v3 = static_cast<double>(input) - state2;
        const double v1 = a1 * state1 + a2 * v3;
        const double v2 = state2 + a2 * state1 + a3 * v3;
        state1 = 2.0 * v1 - state1;
        state2 = 2.0 * v2 - state2;
        return static_cast<float>(v2);
    }
private:
    double state1 = 0.0, state2 = 0.0;
};
}
