#include "mixer-loudness.h"
#include "ebur128.h"
#include <algorithm>
#include <cmath>
namespace augment {
struct LoudnessMeter::State {
    unsigned rate,channels;
    std::vector<int> map;
    ebur128_state *meter=nullptr;
    uint64_t frames=0;
    ~State(){if(meter)ebur128_destroy(&meter);}
};
LoudnessMeter::LoudnessMeter(unsigned rate,unsigned channels,std::vector<int> map)
    :s_(new State{rate,channels,std::move(map)}){reset();}
LoudnessMeter::~LoudnessMeter()=default;
bool LoudnessMeter::valid() const{return s_->meter!=nullptr;}
bool LoudnessMeter::reset(){
    if(s_->meter)ebur128_destroy(&s_->meter);
    s_->frames=0;
    if(!s_->channels||s_->channels>8||!s_->rate||s_->rate>192000||
       ((s_->channels>2||!s_->map.empty())&&s_->map.size()!=s_->channels))return false;
    s_->meter=ebur128_init(s_->channels,s_->rate,EBUR128_MODE_M|EBUR128_MODE_S|
        EBUR128_MODE_I|EBUR128_MODE_LRA|EBUR128_MODE_TRUE_PEAK|EBUR128_MODE_HISTOGRAM);
    if(!s_->meter)return false;
    for(unsigned i=0;i<s_->map.size();++i)if(ebur128_set_channel(s_->meter,i,s_->map[i])!=EBUR128_SUCCESS){
        ebur128_destroy(&s_->meter);return false;
    }
    return true;
}
bool LoudnessMeter::add(const float *data,size_t frames){
    if(!valid()||!data||!frames)return false;
    if(ebur128_add_frames_float(s_->meter,data,frames)!=EBUR128_SUCCESS)return false;
    s_->frames+=frames;return true;
}
LoudnessReading LoudnessMeter::read()const{
    LoudnessReading r;
    if(!valid())return r;
    r.frames=s_->frames;
    if(r.frames>=s_->rate*4/10){
        ebur128_loudness_momentary(s_->meter,&r.momentary);
        ebur128_loudness_global(s_->meter,&r.integrated);
    }
    if(r.frames>=s_->rate*3){
        ebur128_loudness_shortterm(s_->meter,&r.shortTerm);
        if(std::isfinite(r.integrated))ebur128_loudness_range(s_->meter,&r.range);
    }
    double peak=0;
    for(unsigned c=0;c<s_->channels;++c){
        double p=0;if(ebur128_true_peak(s_->meter,c,&p)==EBUR128_SUCCESS)peak=std::max(peak,p);
    }
    if(peak>0)r.truePeak=20*std::log10(peak);
    return r;
}
}
