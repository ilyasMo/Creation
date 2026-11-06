#pragma once
#include <algorithm>
#include <cmath>

namespace afterglow
{
// Topology-preserving state-variable low-pass, configured as Butterworth (Q=1/sqrt(2)).
// Smoothed integrator conductance keeps the feedback tone useful across sample rates.
class FeedbackFilter
{
public:
    void reset() noexcept { state1 = state2 = 0.0; }
    static float conductance(double rate, float frequency) noexcept
    {
        const double f = std::clamp(static_cast<double>(frequency), 20.0, rate * 0.45);
        return static_cast<float>(std::tan(3.14159265358979323846 * f / rate));
    }
    float process(float input, float g, bool highPass = false) noexcept
    {
        const double conductance = g;
        const double a1 = 1.0 / (1.0 + conductance * (conductance + 1.4142135623730950488));
        const double a2 = conductance * a1, a3 = conductance * a2;
        const double v3 = static_cast<double>(input) - state2;
        const double v1 = a1 * state1 + a2 * v3;
        const double v2 = state2 + a2 * state1 + a3 * v3;
        state1 = 2.0 * v1 - state1;
        state2 = 2.0 * v2 - state2;
        return static_cast<float>(highPass ? static_cast<double>(input) - 1.4142135623730950488 * v1 - v2 : v2);
    }
private:
    double state1 = 0.0, state2 = 0.0;
};
}
