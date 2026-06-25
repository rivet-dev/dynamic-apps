#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
static void mark(const char*m){write(2,m,strlen(m));}
int main(void){
    Display*dpy=XOpenDisplay(":0"); if(!dpy){mark("RF:open_failed\n");return 1;}
    int ev,er; mark(XRenderQueryExtension(dpy,&ev,&er)?"RF:render=yes\n":"RF:render=NO\n");
    char buf[128];
    XRenderPictFormat *a1=XRenderFindStandardFormat(dpy,PictStandardA1);
    XRenderPictFormat *a8=XRenderFindStandardFormat(dpy,PictStandardA8);
    XRenderPictFormat *rgb24=XRenderFindStandardFormat(dpy,PictStandardRGB24);
    XRenderPictFormat *argb32=XRenderFindStandardFormat(dpy,PictStandardARGB32);
    snprintf(buf,sizeof buf,"RF:A1=%p A8=%p RGB24=%p ARGB32=%p\n",(void*)a1,(void*)a8,(void*)rgb24,(void*)argb32);
    mark(buf);
    /* also: does the server report a depth-1 pixmap format at all? */
    int n=0; XPixmapFormatValues *pf=XListPixmapFormats(dpy,&n);
    snprintf(buf,sizeof buf,"RF:pixmap_formats=%d depths:",n); mark(buf);
    for(int i=0;i<n;i++){snprintf(buf,sizeof buf," %d",pf->depth);mark(buf);pf++;}
    mark("\n");
    return 0;
}
