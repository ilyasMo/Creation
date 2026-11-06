#include "../Source/FeedbackColour.h"
#include "../Source/ParameterText.h"
#include <iostream>
#include <stdexcept>
constexpr double pi = 3.14159265358979323846;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()
{
    for(double rate:{44100.0,48000.0,96000.0,192000.0})
        for(bool highPass:{false,true})
        {
            afterglow::FeedbackFilter filter;
            const float g=afterglow::FeedbackFilter::conductance(rate,4000);
            double in=0,out=0;
            for(int i=0;i<static_cast<int>(rate);++i)
            {
                const float x=static_cast<float>(std::sin(2*pi*4000*i/rate));
                const float y=filter.process(x,g,highPass);
                if(i>static_cast<int>(rate*.2)){in+=x*x;out+=y*y;}
            }
            require(std::abs(10*std::log10(out/in)+3.0103)<.1,"EQ cutoff is not sample-rate consistent");
        }
    double value=0;
    require(afterglow::parseParameterText("2.5 kHz","Hz",value)&&value==2500,"Frequency entry failed");
    require(afterglow::parseParameterText("-3,5 dB","dB",value)&&value==-3.5,"Decimal-comma entry failed");
    require(afterglow::parseParameterText("45 %","%",value)&&value==45,"Percent entry failed");
    require(afterglow::parseParameterText("320","ms",value)&&value==320,"Plain value entry failed");
    for(const auto* text:{"abc","NaN","Inf","1e999","20 nonsense"})
        require(!afterglow::parseParameterText(text,"Hz",value),"Invalid value accepted");
    std::cout<<"Passed LP/HP cutoff consistency and typed numeric values, units, commas and invalid text.\n";
}
