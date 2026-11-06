#include "../Source/FeedbackColour.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

constexpr double pi = 3.14159265358979323846;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
double magnitude(const std::vector<float>& data, double frequency)
{
    double real = 0, imaginary = 0;
    for (size_t i=0;i<data.size();++i)
    {
        const double phase = 2*pi*frequency*static_cast<double>(i)/48000.0;
        real += data[i]*std::cos(phase); imaginary -= data[i]*std::sin(phase);
    }
    return std::sqrt(real*real+imaginary*imaginary)/static_cast<double>(data.size());
}
int main()
{
    for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const double frequency = 4000;
        afterglow::FeedbackLowPass filter;
        const float g = afterglow::FeedbackLowPass::conductance(rate, static_cast<float>(frequency));
        double inputPower=0, outputPower=0;
        for(int i=0;i<static_cast<int>(rate);++i)
        {
            const float input=static_cast<float>(std::sin(2*pi*frequency*i/rate));
            const float output=filter.process(input,g);
            if(i>static_cast<int>(rate*.2)){inputPower+=input*input;outputPower+=output*output;}
        }
        const double db=10*std::log10(outputPower/inputPower);
        require(std::abs(db+3.0103)<.1, "Feedback tone corner changes with sample rate");
    }
    afterglow::FeedbackSaturator saturation;
    std::vector<float> plain(48000), filtered(48000);
    for(int i=-4800;i<48000;++i)
    {
        const float input=.9f*static_cast<float>(std::sin(2*pi*10000*i/48000.0));
        const float output=saturation.process(input,3);
        if(i>=0){plain[static_cast<size_t>(i)]=std::tanh(3*input)/3;filtered[static_cast<size_t>(i)]=output;}
    }
    double directAliases=0, reducedAliases=0;
    for(const double frequency : { 18000.0, 2000.0, 22000.0, 6000.0, 14000.0 })
    {
        directAliases+=std::pow(magnitude(plain,frequency),2);
        reducedAliases+=std::pow(magnitude(filtered,frequency),2);
    }
    const double directRatio=directAliases/std::pow(magnitude(plain,10000),2);
    const double reducedRatio=reducedAliases/std::pow(magnitude(filtered,10000),2);
    require(reducedRatio<directRatio*.5, "Saturator does not reduce measured foldback");
    saturation.reset();
    for(int i=0;i<10000;++i)
    {
        const float output=saturation.process(.000001f,static_cast<float>(i%400)/100);
        require(std::isfinite(output)&&std::abs(output)<.00001f,"Low-level saturation unstable");
    }
    std::cout<<"Passed sample-rate-consistent tone and low-level stability; measured foldback change "
             <<10*std::log10(reducedRatio/directRatio)<<" dB.\n";
}
