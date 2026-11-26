#include "mixer-loudness.h"
#include <cmath>
#include <iostream>
#include <vector>
#define REQUIRE(x) do {if(!(x)){std::cerr<<"failed "<<#x<<" at "<<__LINE__<<"\n";return 1;}}while(0)
int main(){
 using namespace augment;
 LoudnessMeter meter(48000,2);
 REQUIRE(meter.valid());
 REQUIRE(!std::isfinite(meter.read().momentary));
 std::vector<float> samples(48000*2);
 for(size_t i=0;i<48000;++i) samples[i*2]=samples[i*2+1]=.1f*std::sin(2*3.141592653589793*1000*i/48000);
 for(int i=0;i<6;++i)REQUIRE(meter.add(samples.data(),48000));
 auto r=meter.read();
 REQUIRE(std::abs(r.momentary+20)<.3);
 REQUIRE(std::abs(r.shortTerm+20)<.3);
 REQUIRE(std::abs(r.integrated+20)<.3);
 REQUIRE(std::abs(r.truePeak+20)<.2);
 REQUIRE(r.range<.2);
 REQUIRE(meter.reset());
 REQUIRE(!std::isfinite(meter.read().integrated));
 REQUIRE(!std::isfinite(meter.read().truePeak));
 std::fill(samples.begin(),samples.end(),0);
 for(int i=0;i<4;++i)REQUIRE(meter.add(samples.data(),48000));
 REQUIRE(!std::isfinite(meter.read().momentary));
 REQUIRE(!std::isfinite(meter.read().integrated));
 REQUIRE(!std::isfinite(meter.read().truePeak));
 std::cout<<"Loudness sine, silence and reset verified\n";
}
