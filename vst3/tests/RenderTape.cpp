#include "../Source/DelayEngine.h"
#include <fstream>
#include <iostream>
#include <string>
int main(int argc,char** argv)
{
    if(argc!=2){std::cerr<<"Usage: afterglow_tape_render OUTPUT_PREFIX\n";return 1;}
    constexpr int rate=48000,count=34*rate;
    for(int mode=0;mode<3;++mode)
    {
        afterglow::DelaySettings settings;settings.tapeEnabled=true;
        if(mode==1)settings.wow=.5f;if(mode==2)settings.flutter=.5f;
        afterglow::DelayEngine engine;engine.prepare(rate,settings);
        std::ofstream out(std::string(argv[1])+(mode==0?"-Baseline.f32":mode==1?"-Wow.f32":"-Flutter.f32"),std::ios::binary);
        if(!out)return 2;
        constexpr int block=127;std::array<float,block> left{},right{};
        for(int start=0;start<count;start+=block)
        {
            const int n=std::min(block,count-start);
            for(int i=0;i<n;++i)
            {
                const int sample=start+i-rate;
                double value=0;
                if(sample>=0&&sample<30*rate)
                {
                    double ramp=1;constexpr int fade=960;
                    if(sample<fade)ramp=.5-.5*std::cos(3.14159265358979323846*sample/(fade-1));
                    if(sample>=30*rate-fade)ramp=.5-.5*std::cos(3.14159265358979323846*(30*rate-1-sample)/(fade-1));
                    value=std::pow(10.0,-18.0/20)*std::sin(2*3.14159265358979323846*3150*sample/rate)*ramp;
                }
                left[static_cast<size_t>(i)]=right[static_cast<size_t>(i)]=static_cast<float>(value);
            }
            float* channels[]{left.data(),right.data()};engine.process(channels,2,n,settings);
            for(int i=0;i<n;++i){out.write(reinterpret_cast<const char*>(&left[static_cast<size_t>(i)]),sizeof(float));out.write(reinterpret_cast<const char*>(&right[static_cast<size_t>(i)]),sizeof(float));}
        }
    }
    std::cout<<"Rendered independent Tape, Delay OFF, Master Mix 100%, 48 kHz / stereo float.\n";
}
