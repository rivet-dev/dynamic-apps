/* libepoxy is built without GLX (software X server, no OpenGL). GDK's GLX backend references the
 * epoxy_glX* dispatch pointers as data; define them -> a zero-returning stub so glXQueryExtension
 * reports no GLX and GDK falls back to software. Platform shim. */
typedef long (*glx_fn)(void);
static long glx_zero(void) { return 0; }
glx_fn epoxy_glXChooseFBConfig = glx_zero;
glx_fn epoxy_glXCreatePixmap = glx_zero;
glx_fn epoxy_glXDestroyPixmap = glx_zero;
glx_fn epoxy_glXGetClientString = glx_zero;
glx_fn epoxy_glXGetConfig = glx_zero;
glx_fn epoxy_glXGetFBConfigAttrib = glx_zero;
glx_fn epoxy_glXGetFBConfigs = glx_zero;
glx_fn epoxy_glXGetProcAddress = glx_zero;
glx_fn epoxy_glXGetProcAddressARB = glx_zero;
glx_fn epoxy_glXGetVisualFromFBConfig = glx_zero;
glx_fn epoxy_glXMakeContextCurrent = glx_zero;
glx_fn epoxy_glXMakeCurrent = glx_zero;
glx_fn epoxy_glXQueryDrawable = glx_zero;
glx_fn epoxy_glXQueryExtension = glx_zero;
glx_fn epoxy_glXQueryExtensionsString = glx_zero;
glx_fn epoxy_glXQueryVersion = glx_zero;
glx_fn epoxy_glXSwapIntervalSGI = glx_zero;
glx_fn epoxy_glXSwapIntervalEXT = glx_zero;
glx_fn epoxy_glXCreateNewContext = glx_zero;
glx_fn epoxy_glXCreateContext = glx_zero;
glx_fn epoxy_glXCreateContextAttribsARB = glx_zero;
glx_fn epoxy_glXDestroyContext = glx_zero;
glx_fn epoxy_glXIsDirect = glx_zero;
glx_fn epoxy_glXSwapBuffers = glx_zero;
glx_fn epoxy_glXWaitGL = glx_zero;
glx_fn epoxy_glXWaitX = glx_zero;
glx_fn epoxy_glXChooseVisual = glx_zero;
glx_fn epoxy_glXGetCurrentContext = glx_zero;
glx_fn epoxy_glXGetCurrentDisplay = glx_zero;
