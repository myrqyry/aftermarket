#include "source-performance.h"
#include <obs.h>
#include <QCoreApplication>
#include <iostream>
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
static const char *name(void*){return "Performance test";}
static void *create(obs_data_t*,obs_source_t*){return new int(1);}
static void destroy(void *p){delete static_cast<int*>(p);}
static obs_source_frame *video(void*,obs_source_frame *frame){return frame;}
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);using augment::SourcePerformance;
 REQUIRE(obs_startup("en-US",nullptr,nullptr));
 obs_source_info input{};input.id="am-perf-test-input";input.type=OBS_SOURCE_TYPE_INPUT;
 input.output_flags=OBS_SOURCE_ASYNC_VIDEO;input.get_name=name;input.create=create;input.destroy=destroy;
 obs_register_source(&input);
 auto *source=obs_source_create(input.id,"Async",nullptr,nullptr);REQUIRE(source);
 const QString id=QString::fromUtf8(obs_source_get_uuid(source));
 REQUIRE(!SourcePerformance::available()&&!SourcePerformance::setInterval(id,100));
 obs_source_info filter=input;filter.id="net.nagater.obs-frame-interleave-filter.filter";filter.type=OBS_SOURCE_TYPE_FILTER;filter.filter_video=video;
 obs_register_source(&filter);
 REQUIRE(SourcePerformance::available());
 REQUIRE(SourcePerformance::inspect(id).eligible);
 REQUIRE(SourcePerformance::setInterval(id,100));
 auto state=SourcePerformance::inspect(id);REQUIRE(state.managed&&state.intervalMs==100&&!state.blocked);
 REQUIRE(SourcePerformance::setInterval(id,500));
 REQUIRE(SourcePerformance::inspect(id).intervalMs==500);
 REQUIRE(!SourcePerformance::setInterval(id,1001));
 REQUIRE(SourcePerformance::setInterval(id,0));
 REQUIRE(!SourcePerformance::inspect(id).managed);
 auto *external=obs_source_create_private(filter.id,"Manual filter",nullptr);REQUIRE(external);
 obs_source_filter_add(source,external);
 REQUIRE(SourcePerformance::inspect(id).blocked);
 REQUIRE(!SourcePerformance::setInterval(id,100)&&!SourcePerformance::setInterval(id,0));
 REQUIRE(obs_filter_get_parent(external)==source);
 obs_source_filter_remove(source,external);obs_source_release(external);
 REQUIRE(!SourcePerformance::setInterval("stale",100));
 obs_source_remove(source);obs_source_release(source);
 obs_queue_task(OBS_TASK_DESTROY,[](void*){},nullptr,true);
 obs_shutdown();
 std::cout<<"SOURCE_PERFORMANCE_PASS\n";
}
