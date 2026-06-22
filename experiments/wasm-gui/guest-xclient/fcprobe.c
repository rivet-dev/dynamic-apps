#include <stdio.h>
#include <fontconfig/fontconfig.h>
static void m(const char*s){ fprintf(stderr,"%s",s); fflush(stderr); }
int main(void){
    m("FC:start\n");
    FcBool ok = FcInit();
    fprintf(stderr,"FC:FcInit=%d\n", ok); fflush(stderr);
    FcConfig *cfg = FcConfigGetCurrent();
    fprintf(stderr,"FC:config=%p\n", (void*)cfg); fflush(stderr);
    FcPattern *pat = FcNameParse((const FcChar8*)"sans");
    FcConfigSubstitute(cfg, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);
    FcResult res; FcPattern *m2 = FcFontMatch(cfg, pat, &res);
    fprintf(stderr,"FC:match=%p res=%d\n", (void*)m2, res); fflush(stderr);
    m("FC:done\n");
    return 0;
}
