#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>
namespace augment {
struct LoudnessReading {
    double momentary=-std::numeric_limits<double>::infinity();
    double shortTerm=momentary, integrated=momentary, range=momentary, truePeak=momentary;
    uint64_t frames=0;
};
class LoudnessMeter {
public:
    LoudnessMeter(unsigned rate,unsigned channels,std::vector<int> map={});
    ~LoudnessMeter();
    bool valid() const;
    bool reset();
    bool add(const float *interleaved,size_t frames);
    LoudnessReading read() const;
private:
    struct State;
    std::unique_ptr<State> s_;
};
}
