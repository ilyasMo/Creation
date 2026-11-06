#include "../Source/DelayEngine.h"
#include <iostream>
#include <stdexcept>

void check(bool v, const char* s) { if (!v) throw std::runtime_error(s); }
using Audio = std::array<std::vector<float>, 2>;
Audio render(afterglow::DelaySettings s, double rate, Audio input, int channels = 2)
{
    afterglow::DelayEngine engine; engine.prepare(rate, s);
    const auto length = static_cast<int>(input[0].size());
    for (int offset = 0; offset < length; offset += 127)
    {
        float* ptr[] { input[0].data() + offset, input[1].data() + offset };
        engine.process(ptr, channels, std::min(127, length - offset), s);
    }
    return input;
}
Audio empty(int n) { return { std::vector<float>(static_cast<size_t>(n)), std::vector<float>(static_cast<size_t>(n)) }; }
double energy(const std::vector<float>& v, int start, int end)
{
    double e = 0; for (int i = start; i < end; ++i) e += v[static_cast<size_t>(i)] * v[static_cast<size_t>(i)]; return e;
}
int main()
{
    afterglow::DelaySettings s; s.sync = true; s.division = 3; s.bpm = 120;
    check(std::abs(afterglow::delaySeconds(s) - 0.25) < 1e-9, "Straight sync incorrect");
    s.timing = 1; check(std::abs(afterglow::delaySeconds(s) - 0.375) < 1e-9, "Dotted sync incorrect");
    s.timing = 2; check(std::abs(afterglow::delaySeconds(s) - 1.0/6.0) < 1e-9, "Triplet sync incorrect");
    s.bpm = 240; s.division = 6; s.timing = 0;
    check(std::abs(afterglow::delaySeconds(s) - 0.015625) < 1e-9, "Fast 64th note incorrectly clamped");
    s.bpm = 0; check(std::isfinite(afterglow::delaySeconds(s)), "Missing tempo unsafe");
    s.bpm = 0.1; s.division = 0; check(afterglow::delaySeconds(s) <= 24.0, "Delay capacity exceeded");
    s = {}; s.enabled = true; s.sync = true; s.bpm = 240; s.division = 6; s.feedback = 0;
    auto fastImpulse = empty(4800); fastImpulse[0][0] = 1;
    auto fastOutput = render(s, 48000, fastImpulse);
    check(std::abs(fastOutput[0][750] - .5f) < 1e-4f, "Fast synced echo timing incorrect");
    for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        s = {}; s.timeMs = 40; s.feedback = 0; s.drive = 0;
        auto input = empty(static_cast<int>(rate)); input[0][0] = input[1][0] = 1;
        auto off = render(s, rate, input); check(off == input, "Off is not unity");
        s.enabled = true; s.mix = 0;
        check(render(s, rate, input) == input, "Master zero is not unity");
        s.mix = 1; auto echoes = render(s, rate, input);
        const int at = static_cast<int>(rate * 0.04);
        check(echoes[0][0] == 1 && std::abs(echoes[0][at] - 0.5f) < 1e-4f, "Echo timing/amplitude wrong");
        auto mono = render(s, rate, input, 1);
        check(std::abs(mono[0][at] - 0.5f) < 1e-4f, "Mono delay failed");
    }
    s = {}; s.enabled = true; s.timeMs = 100; s.feedback = .75f; s.drive = 0; s.toneHz = 1500;
    auto echoRatio = [&](double frequency) {
        auto input = empty(48000);
        for (int i = 0; i < 960; ++i) input[0][i] = input[1][i] = .1f * static_cast<float>(std::sin(i * frequency * 6.283185307179586 / 48000.0));
        auto out = render(s, 48000, input);
        return energy(out[0], 9600, 10560) / energy(out[0], 4800, 5760);
    };
    check(echoRatio(10000) < echoRatio(500) * .2, "Feedback repeats do not darken");
    s.pingPong = 1; auto impulse = empty(48000); impulse[0][0] = 1;
    auto bounce = render(s, 48000, impulse);
    check(energy(bounce[0], 4800, 4900) > .01 && energy(bounce[1], 4800, 4900) < 1e-8, "First ping-pong side wrong");
    check(energy(bounce[1], 9600, 10000) > .0001, "Ping-pong cross-feedback failed");
    afterglow::DelayEngine engine; s = {}; s.enabled = true; s.feedback = .95f; engine.prepare(48000, s);
    float last = 0;
    for (int block = 0; block < 1000; ++block)
    {
        auto audio = empty(128);
        for (int i = 0; i < 128; ++i) audio[0][i] = audio[1][i] = .1f * std::sin(static_cast<float>(block*128+i) * .08f);
        if (block % 7 == 0) { s.timeMs = 20.0f + static_cast<float>(block % 1900); s.toneHz = block % 2 == 0 ? 500.0f : 14000.0f; }
        s.drive = 1; s.pingPong = .8f;
        float* ptr[] { audio[0].data(), audio[1].data() }; engine.process(ptr, 2, 128, s);
        for (float v : audio[0]) { check(std::isfinite(v) && std::abs(v) < 2, "Feedback unstable"); check(std::abs(v-last) < .5f, "Retiming discontinuity"); last = v; }
    }
    std::cout << "Passed tempo divisions, mono/stereo unity, echo timing, darkening feedback, ping-pong and automation stress.\n";
}
