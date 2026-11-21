#include "obs-transform.h"
#include <QApplication>
#include <QPalette>
#include <QTemporaryDir>
#include <util/config-file.h>
#include <iostream>
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAILED "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
int main(int argc,char **argv){
 QApplication app(argc,argv);
 QTemporaryDir dir;
 const auto configPath=dir.filePath("native-colors.ini").toUtf8();
 config_t *cfg=config_create(configPath.constData());
 REQUIRE(cfg);
 config_set_bool(cfg,"Accessibility","OverrideColors",false);
 config_set_int(cfg,"Accessibility","SelectRed",255);
 QPalette a; a.setColor(QPalette::Link,QColor("#5ac6d6"));
 a.setColor(QPalette::Highlight,QColor("#bd83f0"));
 augment::NativeTransformColors colors(cfg);
 REQUIRE(colors.apply(a));
 REQUIRE(config_get_bool(cfg,"Accessibility","OverrideColors"));
 REQUIRE(config_get_int(cfg,"Accessibility","SelectRed")!=255);
 REQUIRE(config_has_user_value(cfg,"Accessibility","SelectGreen"));
 auto first=config_get_int(cfg,"Accessibility","SelectRed");
 QPalette b=a;b.setColor(QPalette::Link,QColor("#f29b85"));
 REQUIRE(colors.apply(b));
 REQUIRE(config_get_int(cfg,"Accessibility","SelectRed")!=first);
 colors.restore();
 REQUIRE(!config_get_bool(cfg,"Accessibility","OverrideColors"));
 REQUIRE(config_get_int(cfg,"Accessibility","SelectRed")==255);
 REQUIRE(!config_has_user_value(cfg,"Accessibility","SelectGreen"));
 REQUIRE(!config_has_user_value(cfg,"Accessibility","SelectBlue"));
 // OBS declares defaults before plugins run. Removing a user override must
 // restore the effective default, not force OverrideColors on.
 config_set_default_bool(cfg,"Accessibility","OverrideColors",false);
 REQUIRE(colors.apply(a));
 config_set_int(cfg,"Accessibility","SelectRed",123456);
 REQUIRE(!colors.apply(b));
 REQUIRE(config_get_int(cfg,"Accessibility","SelectRed")==123456); // respect user's external edit
 REQUIRE(!config_get_bool(cfg,"Accessibility","OverrideColors"));
 REQUIRE(!colors.apply(a)); // no subsequent timer tick may re-clobber the user
 colors.resetBlocked();
 config_close(cfg);
 std::cout<<"native color override theme update, missing keys, and non-destructive restoration passed\n";
}
