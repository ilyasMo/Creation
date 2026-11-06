#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace afterglow
{
// Independent, stereo-linked pitch movement. No saturation, Age model or intentional tone EQ.
// A constant six-millisecond dry reference permits host latency compensation and aligned mixing.
class TapeEngine
{
public:
    struct Frame { std::array<float,2> dry{}, wet{}; };

    void prepare(double sampleRate, bool enabled, float wowAmount, float flutterAmount)
    {
        rate = std::isfinite(sampleRate) && sampleRate >= 8000.0 ? sampleRate : 48000.0;
        latency = static_cast<int>(std::llround(rate * 0.006));
        size_t length = 1;
        while (length < static_cast<size_t>(std::ceil(rate * 0.032)) + 64) length *= 2;
        mask = length - 1;
        for (auto& channel : ring) channel.assign(length,0.0f);
        write = 0;
        makeKernel();
        constexpr std::array<double,3> cutoffs{0.18,3.7,25.0};
        constexpr std::array<std::uint32_t,3> seeds{0x51f2a97du,0x9a35b817u,0x4d713efbu};
        for (size_t i=0;i<noise.size();++i)
        {
            noise[i].prepare(cutoffs[i],seeds[i]);
            // Warm the slowest filter before playback, rather than fading in a statistical transient.
            for (int n=0;n<30000;++n) noise[i].next();
            for (auto& point : points) point[i]=noise[i].next();
        }
        phase = 0;
        wow = safe(wowAmount); flutter = safe(flutterAmount); active = enabled ? 1.0 : 0.0;
        amountSmoothing = 1.0-std::exp(-1.0/(rate*0.080));
        bypassSmoothing = 1.0-std::exp(-1.0/(rate*0.020));
        lastOffset = displacement();
        maxExcursion = std::max(0.0,std::min(rate*0.005,static_cast<double>(latency)-(taps/2+4.0)));
        lastOffset = std::clamp(lastOffset,-maxExcursion,maxExcursion);
    }

    int latencySamples() const noexcept { return latency; }
    double modulationSeconds() const noexcept { return lastOffset/rate; }

    Frame process(float left, float right, bool enabled, float wowAmount, float flutterAmount) noexcept
    {
        if (ring[0].empty()) return {{left,right},{left,right}};
        approach(wow,safe(wowAmount),amountSmoothing);
        approach(flutter,safe(flutterAmount),amountSmoothing);
        approach(active,enabled?1.0:0.0,bypassSmoothing);
        // Depth continues evolving during bypass; switching bypass does not yank the read head to zero.
        const double wanted = std::clamp(displacement(),-maxExcursion,maxExcursion);
        lastOffset += std::clamp(wanted-lastOffset,-0.04,0.04);
        ring[0][write]=left; ring[1][write]=right;
        const auto dryIndex=(write+ring[0].size()-static_cast<size_t>(latency))&mask;
        Frame result;
        result.dry={ring[0][dryIndex],ring[1][dryIndex]};
        result.wet=result.dry;
        if (active>0.0 && (wow>0.0 || flutter>0.0))
        {
            const auto moved=read(static_cast<double>(latency)+lastOffset);
            for(size_t ch=0;ch<2;++ch)
                result.wet[ch]=result.dry[ch]+static_cast<float>(active)*(moved[ch]-result.dry[ch]);
        }
        write=(write+1)&mask;
        phase+=1000.0/rate;
        while(phase>=1.0)
        {
            phase-=1.0; points[0]=points[1];points[1]=points[2];points[2]=points[3];
            for(size_t i=0;i<noise.size();++i)points[3][i]=noise[i].next();
        }
        return result;
    }

private:
    static constexpr double pi=3.1415926535897932384626433832795;
    static constexpr int taps=64, phases=1024, leftTaps=31;
    struct Biquad
    {
        double b0=0,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
        void prepare(double frequency,double q)
        {
            const double w=2*pi*frequency/1000.0,c=std::cos(w),alpha=std::sin(w)/(2*q),den=1+alpha;
            b0=(1-c)*.5/den;b1=(1-c)/den;b2=b0;a1=-2*c/den;a2=(1-alpha)/den;z1=z2=0;
        }
        double process(double x) noexcept
        {const double y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y;}
    };
    struct BandNoise
    {
        std::array<Biquad,3> filters;
        std::uint32_t rng=1;
        double normalisation=1;
        void prepare(double frequency,std::uint32_t seed)
        {
            rng=seed;
            constexpr std::array<double,3> q{0.5176380902050415,0.7071067811865476,1.9318516525781366};
            for(size_t i=0;i<filters.size();++i)filters[i].prepare(frequency,q[i]);
            // Integrated power of a sixth-order Butterworth LP driven by unit-variance noise.
            const double bandwidth=frequency*pi/(12*std::sin(pi/12));
            normalisation=1/std::sqrt(2*bandwidth/1000.0);
        }
        double next() noexcept
        {
            rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;
            double value=(static_cast<double>(rng>>8)/8388608.0-1.0)*1.7320508075688772;
            for(auto& filter:filters)value=filter.process(value);
            return value*normalisation;
        }
    };
    static double safe(float value) noexcept
    {return std::isfinite(value)?std::clamp(static_cast<double>(value),0.0,1.0):0.0;}
    static void approach(double& value,double target,double amount) noexcept
    {value+=amount*(target-value);if(std::abs(value-target)<1e-10)value=target;}
    static double cubic(double a,double b,double c,double d,double x) noexcept
    {return b+.5*x*(c-a+x*(2*a-5*b+4*c-d+x*(3*(b-c)+d-a)));}
    double displacement() const noexcept
    {
        std::array<double,3> value{};
        for(size_t i=0;i<value.size();++i)
        {
            const double raw=cubic(points[0][i],points[1][i],points[2][i],points[3][i],phase);
            // Bounded and differentiable: a rare random excursion cannot hit a buffer boundary.
            value[i]=2.4*std::tanh(raw*.5);
        }
        // First-version 50% targets: approximately 5 cents RMS Wow and 7 cents RMS Flutter.
        const double seconds=2*wow*(.0007*value[0]+.0002*value[1])+2*flutter*.000043*value[2];
        return seconds*rate;
    }
    void makeKernel()
    {
        kernel.resize(static_cast<size_t>(phases+1)*taps);
        std::array<double,taps> window{};
        for(int n=0;n<taps;++n)
        {
            const double p=2*pi*n/(taps-1);
            window[static_cast<size_t>(n)]=.42-.5*std::cos(p)+.08*std::cos(2*p);
        }
        for(int phaseIndex=0;phaseIndex<=phases;++phaseIndex)
        {
            const double fraction=static_cast<double>(phaseIndex)/phases;
            std::array<double,taps> coefficients{};double sum=0;
            for(int n=0;n<taps;++n)
            {
                const double x=n-leftTaps-fraction;
                const double sinc=std::abs(x)<1e-12?1.0:std::sin(pi*x)/(pi*x);
                coefficients[static_cast<size_t>(n)]=sinc*window[static_cast<size_t>(n)];sum+=coefficients[static_cast<size_t>(n)];
            }
            for(int n=0;n<taps;++n)
                kernel[static_cast<size_t>(phaseIndex*taps+n)]=static_cast<float>(coefficients[static_cast<size_t>(n)]/sum);
        }
    }
    std::array<float,2> read(double delay) const noexcept
    {
        const double position=static_cast<double>(write)-delay;
        const double integral=std::floor(position),fraction=position-integral;
        const double tablePosition=fraction*phases;
        const int index=std::min(phases-1,static_cast<int>(tablePosition));
        const float blend=static_cast<float>(tablePosition-index);
        const auto start=static_cast<std::int64_t>(integral)-leftTaps;
        const auto row=static_cast<size_t>(index*taps);
        std::array<double,2> sums{};
        for(int n=0;n<taps;++n)
        {
            const auto k=row+static_cast<size_t>(n);
            const float weight=kernel[k]+(kernel[k+taps]-kernel[k])*blend;
            const auto sample=static_cast<size_t>(start+n)&mask;
            sums[0]+=weight*ring[0][sample];sums[1]+=weight*ring[1][sample];
        }
        return {static_cast<float>(sums[0]),static_cast<float>(sums[1])};
    }
    std::array<std::vector<float>,2> ring;
    std::vector<float> kernel;
    std::array<BandNoise,3> noise;
    std::array<std::array<double,3>,4> points{};
    size_t write=0,mask=0;
    int latency=288;
    double rate=48000,phase=0,lastOffset=0,maxExcursion=240;
    double wow=0,flutter=0,active=0,amountSmoothing=.001,bypassSmoothing=.001;
};
}
