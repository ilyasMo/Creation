#include "../Source/TapeEngine.h"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace { bool track=false;size_t allocations=0; }
void* operator new(std::size_t n) {if(track)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
constexpr double pi=3.14159265358979323846;
int main()
{
    for(double rate:{44100.0,48000.0,96000.0,192000.0})
    {
        afterglow::TapeEngine bypass,neutral;
        bypass.prepare(rate,false,1,1);neutral.prepare(rate,true,0,0);
        const int latency=bypass.latencySamples();
        check(latency==static_cast<int>(std::llround(rate*.006)),"Wrong fixed Tape latency");
        for(int i=0;i<latency+100;++i)
        {
            const float input=i==0?1.0f:0.0f;
            const auto b=bypass.process(input,-input,false,1,1),n=neutral.process(input,-input,true,0,0);
            const float expected=i==latency?1.0f:0.0f;
            check(b.wet[0]==expected&&b.wet[1]==-expected,"Tape bypass is not an exact aligned passthrough");
            check(n.wet==b.wet,"Zero-depth Tape changes the signal");
        }
        // Measure the actual read-head speed in physical units, independently of interpolation.
        for(bool slow:{true,false})
        {
            afterglow::TapeEngine movement;movement.prepare(rate,false,slow?.5f:0,slow?0:.5f);
            double previous=movement.modulationSeconds(),sum=0,sum2=0;int count=0;
            for(int i=0;i<static_cast<int>(rate*30);++i)
            {
                movement.process(0,0,false,slow?.5f:0,slow?0:.5f);
                const double now=movement.modulationSeconds();
                const double cents=1200*std::log2(1-(now-previous)*rate);previous=now;
                if(i>rate*2){sum+=cents;sum2+=cents*cents;++count;}
                check(std::abs(now)<.00501,"Movement escaped bounded read region");
            }
            const double rms=std::sqrt(sum2/count-std::pow(sum/count,2));
            std::cout<<(slow?"Wow":"Flutter")<<" at "<<rate<<" Hz: "<<rms<<" cents RMS\n";
            check(slow?(rms>3.7&&rms<6.3):(rms>5.4&&rms<8.8),"Movement depth is outside first-version calibration");
        }
    }
    // Compare moving reads to an analytically resampled sine near the top of the audible band.
    for(double rate:{44100.0,48000.0})
    {
        afterglow::TapeEngine high;high.prepare(rate,true,.5f,.5f);double error=0,power=0;
        for(int i=0;i<static_cast<int>(rate*2);++i)
        {
            const float sample=.2f*static_cast<float>(std::sin(2*pi*20000*i/rate));
            const auto frame=high.process(sample,sample,true,.5f,.5f);
            if(i>1000)
            {
                const double d=high.latencySamples()+high.modulationSeconds()*rate;
                const double ideal=.2*std::sin(2*pi*20000*(i-d)/rate);
                error+=std::pow(frame.wet[0]-ideal,2);power+=ideal*ideal;
            }
        }
        check(10*std::log10(error/power)<-65,"High-frequency moving-read interpolation is inaccurate");
    }
    afterglow::TapeEngine lowRate;lowRate.prepare(8000,true,1,1);
    for(int i=0;i<8000;++i)
    {
        const auto value=lowRate.process(.2f,.2f,true,1,1);
        if(i>512)check(std::abs(value.wet[0]-.2f)<2e-6,"Low-rate read touches unavailable samples or loses DC gain");
    }
    afterglow::TapeEngine tape;tape.prepare(48000,true,.5f,.5f);
    double energyIn=0,energyOut=0;float last=0;
    for(int i=0;i<144000;++i)
    {
        const float sample=.2f*static_cast<float>(std::sin(2*pi*1000*i/48000));
        const auto frame=tape.process(sample,-sample,true,.5f,.5f);
        check(frame.wet[0]==-frame.wet[1],"Stereo movement damaged anti-phase stereo");
        if(i>48000){energyIn+=sample*sample;energyOut+=frame.wet[0]*frame.wet[0];}
    }
    check(std::abs(10*std::log10(energyOut/energyIn))<.1,"Tape loses or adds steady level");
    // No audio-thread allocations; automation must remain finite and have no sample discontinuities.
    track=true;allocations=0;
    for(int i=0;i<96000;++i)
    {
        const float sample=.2f*static_cast<float>(std::sin(2*pi*1000*i/48000));
        const bool enabled=(i/12000)%2==0;
        const float amount=(i/7000)%2==0?0.0f:1.0f;
        const auto frame=tape.process(sample,sample,enabled,amount,1-amount);
        if(i>48000)check(std::abs(frame.wet[0]-last)<.04f,"Tape control automation clicks");
        check(std::isfinite(frame.wet[0])&&std::abs(frame.wet[0])<.25f,"Tape automation is unstable");last=frame.wet[0];
    }
    track=false;check(allocations==0,"Tape allocates during audio processing");
    std::cout<<"Passed fixed latency, neutral/bypass, calibrated depth, stereo, level and allocation/automation checks.\n";
}
