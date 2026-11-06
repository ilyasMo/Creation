#include "../Source/DelayEngine.h"
#include <iostream>
#include <stdexcept>
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
using Audio=std::array<std::vector<float>,2>;
Audio empty(int n){return {std::vector<float>(static_cast<size_t>(n)),std::vector<float>(static_cast<size_t>(n))};}
Audio render(afterglow::DelaySettings s,double rate,Audio input,int channels=2)
{
    afterglow::DelayEngine engine;engine.prepare(rate,s);
    for(int i=0;i<static_cast<int>(input[0].size());i+=127)
    {
        float* p[]{input[0].data()+i,input[1].data()+i};
        engine.process(p,channels,std::min(127,static_cast<int>(input[0].size())-i),s);
    }
    return input;
}
double energy(const std::vector<float>& v,int start,int end)
{double e=0;for(int i=start;i<end;++i)e+=v[static_cast<size_t>(i)]*v[static_cast<size_t>(i)];return e;}
int main()
{
    afterglow::DelaySettings s;s.sync=true;s.division=3;s.bpm=120;
    check(std::abs(afterglow::delaySeconds(s)-.25)<1e-9,"Straight timing failed");
    s.timing=1;check(std::abs(afterglow::delaySeconds(s)-.375)<1e-9,"Dotted timing failed");
    s.timing=2;check(std::abs(afterglow::delaySeconds(s)-1.0/6)<1e-9,"Triplet timing failed");
    s.bpm=240;s.division=6;s.timing=0;
    check(std::abs(afterglow::delaySeconds(s)-.015625)<1e-9,"Fast sync limited");
    for(double rate:{44100.0,48000.0,96000.0,192000.0})
    {
        s={};s.timeMs=40;s.feedback=0;
        auto input=empty(static_cast<int>(rate));input[0][0]=input[1][0]=1;
        check(render(s,rate,input)==input,"All-off unity failed");
        s.enabled=true;s.eqEnabled=true;s.lowCutHz=2000;s.highCutHz=3000;s.mix=0;
        check(render(s,rate,input)==input,"Master zero does not bypass entire processing");
        s.mix=1;s.eqEnabled=false;const int at=static_cast<int>(rate*.04);
        check(std::abs(render(s,rate,input)[0][at]-.5f)<1e-4,"Echo timing failed");
        check(std::abs(render(s,rate,input,1)[0][at]-.5f)<1e-4,"Mono failed");
    }
    s={};s.enabled=true;s.timeMs=100;s.feedback=0;
    auto source=empty(48000);
    for(int i=0;i<960;++i)source[0][i]=source[1][i]=.1f*static_cast<float>(std::sin(i*200*6.283185307179586/48000));
    auto unfiltered=render(s,48000,source);
    s.eqEnabled=true;s.lowCutHz=2000;s.highCutHz=20000;
    auto filtered=render(s,48000,source);
    check(energy(filtered[0],0,960)<energy(unfiltered[0],0,960)*.02,"EQ does not affect processed core");
    check(energy(filtered[0],4800,5760)<energy(unfiltered[0],4800,5760)*.02,"EQ does not affect first echo");
    s.enabled=false;
    check(energy(render(s,48000,source)[0],0,960)<energy(source[0],0,960)*.02,"EQ stops when delay bypassed");
    s.enabled=true;s.mix=.5f;auto half=render(s,48000,source);
    for(size_t i=0;i<source[0].size();++i)
        check(std::abs(half[0][i]-(source[0][i]+filtered[0][i])*.5f)<1e-6,"Master is not whole-chain dry/wet");
    s={};s.enabled=true;s.timeMs=100;s.feedback=.75f;s.pingPong=1;
    auto pulse=empty(48000);pulse[0][0]=1;auto bounce=render(s,48000,pulse);
    check(energy(bounce[0],4800,4900)>.01&&energy(bounce[1],4800,4900)<1e-8,"Ping-pong first side failed");
    check(energy(bounce[1],9600,10000)>.001,"Ping-pong second side failed");
    auto tone=empty(96000);
    for(size_t i=0;i<tone[0].size();++i)tone[0][i]=tone[1][i]=.1f*std::sin(static_cast<float>(i)*.08f);
    s={};s.enabled=true;s.timeMs=100;s.feedback=.4f;
    const auto baseline=render(s,48000,tone);
    for(bool slow:{false,true})
    {
        auto mod=s;if(slow)mod.wow=1;else mod.flutter=1;
        const auto moved=render(mod,48000,tone);double difference=0;
        for(size_t i=24000;i<tone[0].size();++i)difference+=std::pow(moved[0][i]-baseline[0][i],2);
        check(difference>1,"Wow/flutter does not modulate the echoes");
    }
    afterglow::DelayEngine engine;s={};s.enabled=true;s.feedback=.95f;s.wow=s.flutter=1;s.eqEnabled=true;engine.prepare(48000,s);
    for(int block=0;block<1000;++block)
    {
        auto audio=empty(128);for(int i=0;i<128;++i)audio[0][i]=audio[1][i]=.1f*std::sin(static_cast<float>(block*128+i)*.08f);
        if(block%7==0){s.timeMs=20.0f+static_cast<float>(block%1900);s.lowCutHz=block%2?20.0f:18000.0f;s.highCutHz=block%2?20000.0f:100.0f;}
        s.pingPong=.8f;float* p[]{audio[0].data(),audio[1].data()};engine.process(p,2,128,s);
        for(float v:audio[0])check(std::isfinite(v)&&std::abs(v)<3,"Feedback/modulation unstable");
    }
    // Extreme delay lengths plus modulation must not read outside the buffer.
    for(double bpm:{.1,100000.0}){s.sync=true;s.bpm=bpm;s.division=bpm<1?0:6;auto out=render(s,48000,empty(1000));check(std::isfinite(out[0][0]),"Extreme modulation delay failed");}
    std::cout<<"Passed timing, global EQ/mix, unity, first-echo filtering, wow/flutter, ping-pong and stress.\n";
}
