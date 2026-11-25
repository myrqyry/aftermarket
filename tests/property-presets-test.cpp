#include "property-presets.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);QTemporaryDir dir;REQUIRE(dir.isValid());
 augment::PropertyPresets store(dir.filePath("presets.json"));
 const QByteArray a=R"({"gamma":0.25,"enabled":true,"text":"123"})", b=R"({"gamma":0.5})";
 REQUIRE(store.save("filter:color_filter_v2","Warm",a,false));
 REQUIRE(store.save("input:image_source","Warm",b,false));
 REQUIRE(!store.save("filter:color_filter_v2","Warm",b,false));
 REQUIRE(store.names("filter:color_filter_v2")==QStringList{"Warm"});
 REQUIRE(store.read("filter:color_filter_v2","Warm")==a);
 REQUIRE(store.read("input:image_source","Warm")==b);
 REQUIRE(store.save("filter:color_filter_v2","Warm",b,true));
 REQUIRE(store.rename("filter:color_filter_v2","Warm","Cool"));
 REQUIRE(store.read("filter:color_filter_v2","Cool")==b);
 REQUIRE(!store.read("filter:color_filter_v2","Warm").size());
 REQUIRE(!store.save("filter:color_filter_v2","",a,false));
 REQUIRE(!store.save("filter:color_filter_v2","Bad","[]",false));
 REQUIRE(store.remove("filter:color_filter_v2","Cool"));
 REQUIRE(store.names("filter:color_filter_v2").isEmpty());
 REQUIRE(store.read("input:image_source","Warm")==b);
 augment::PropertyPresets reopened(dir.filePath("presets.json"));REQUIRE(reopened.read("input:image_source","Warm")==b);
 REQUIRE(!(QFile::permissions(dir.filePath("presets.json"))&(QFileDevice::ReadOther|QFileDevice::WriteOther|QFileDevice::ReadGroup|QFileDevice::WriteGroup)));
 const auto exported=dir.filePath("portable.json");
 REQUIRE(store.exportPreset("input:image_source","Warm",exported));
 REQUIRE(!store.exportPreset("input:image_source","Warm",exported));
 REQUIRE(!store.importPreset("filter:color_filter_v2",exported));
 REQUIRE(!store.importPreset("input:image_source",exported));
 augment::PropertyPresets imported(dir.filePath("imported.json"));
 REQUIRE(imported.importPreset("input:image_source",exported));
 REQUIRE(imported.read("input:image_source","Warm")==b);
 REQUIRE(!imported.importPreset("input:image_source",exported));
 REQUIRE(imported.importPreset("input:image_source",exported,"Imported"));
 REQUIRE(imported.read("input:image_source","Imported")==b);
 const QByteArray chain=R"({"filters":[{"id":"color_filter_v2","name":"Warm","enabled":true,"settings":"{\"gamma\":0.25}"}]})";
 REQUIRE(store.save("chain","Grade",chain,false));
 REQUIRE(!store.save("chain","Bad",R"({"filters":[{"id":"test"}]})",false));
 REQUIRE(!store.save("chain","Bad",R"({"filters":[]})",false));
 REQUIRE(store.exportPreset("chain","Grade",dir.filePath("chain.json")));
 REQUIRE(imported.importPreset("chain",dir.filePath("chain.json")));
 REQUIRE(imported.read("chain","Grade")==chain);
 QFile f(dir.filePath("presets.json"));REQUIRE(f.open(QIODevice::WriteOnly|QIODevice::Truncate));f.write("corrupt");f.close();
 REQUIRE(!store.save("input:image_source","Other",b,false));
 REQUIRE(f.open(QIODevice::ReadOnly));REQUIRE(f.readAll()=="corrupt");
 std::cout<<"PROPERTY_PRESETS_PASS\n";
}
