#pragma once
#include <QString>
namespace augment {
struct SourcePerformanceState {
    bool eligible=false,managed=false,blocked=false,enabled=false;
    int intervalMs=0;
};
class SourcePerformance {
public:
    static bool available();
    static SourcePerformanceState inspect(const QString &uuid);
    // Zero removes only our own filter. Never edit externally owned filters.
    static bool setInterval(const QString &uuid,int milliseconds);
};
}
