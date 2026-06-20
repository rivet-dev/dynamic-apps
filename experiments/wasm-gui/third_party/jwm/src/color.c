/**
 * @file color.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Functions to handle loading colors.
 *
 */

#include "jwm.h"
#include "main.h"
#include "color.h"
#include "error.h"
#include "misc.h"
#include "settings.h"

/** Mapping between color types and default values. */
typedef struct {
   ColorType type;
   unsigned int value;
} DefaultColorNode;

unsigned long colors[COLOR_COUNT];
static unsigned long rgbColors[COLOR_COUNT];

GradientDirection gradients[COLOR_COUNT];

/* Map a linear 8-bit RGB space to pixel values. */
static unsigned long *rgbToPixel;

/* Maximum number of colors to allocate for icons. */
static const unsigned MAX_COLORS = 64;

#ifdef USE_XFT
static XftColor *xftColors[COLOR_COUNT] = { NULL };
#endif

#define THEME_TEXT         0xFFFFFF
#define THEME_ACTIVE       0x0077CC
#define THEME_ACTIVE2      0x004488
#define THEME_INACTIVE     0x555555
#define THEME_INACTIVE2    0x333333
#define THEME_INACTIVE3    0x111111
#define THEME_OUTLINE      0x000000

static const DefaultColorNode DEFAULT_COLORS[] = {

   { COLOR_TITLE_FG,                THEME_TEXT        },
   { COLOR_TITLE_ACTIVE_FG,         THEME_TEXT        },

   { COLOR_TITLE_BG1,               THEME_INACTIVE    },
   { COLOR_TITLE_BG2,               THEME_INACTIVE    },
   { COLOR_TITLE_ACTIVE_BG1,        THEME_ACTIVE      },
   { COLOR_TITLE_ACTIVE_BG2,        THEME_ACTIVE      },

   { COLOR_TRAY_FG,                 THEME_TEXT        },
   { COLOR_TRAY_BG1,                THEME_INACTIVE    },
   { COLOR_TRAY_BG2,                THEME_INACTIVE    },
   { COLOR_TRAY_ACTIVE_FG,          THEME_TEXT        },
   { COLOR_TRAY_ACTIVE_BG1,         THEME_ACTIVE      },
   { COLOR_TRAY_ACTIVE_BG2,         THEME_ACTIVE      },

   { COLOR_TASKLIST_FG,             THEME_TEXT        },
   { COLOR_TASKLIST_BG1,            THEME_INACTIVE    },
   { COLOR_TASKLIST_BG2,            THEME_INACTIVE    },
   { COLOR_TASKLIST_ACTIVE_FG,      THEME_TEXT        },
   { COLOR_TASKLIST_ACTIVE_BG1,     THEME_ACTIVE      },
   { COLOR_TASKLIST_ACTIVE_BG2,     THEME_ACTIVE      },
   { COLOR_TASKLIST_MINIMIZED_FG,   THEME_TEXT        },
   { COLOR_TASKLIST_MINIMIZED_BG1,  THEME_INACTIVE3   },
   { COLOR_TASKLIST_MINIMIZED_BG2,  THEME_INACTIVE3   },

   { COLOR_TRAYBUTTON_FG,           THEME_TEXT        },
   { COLOR_TRAYBUTTON_BG1,          THEME_INACTIVE    },
   { COLOR_TRAYBUTTON_BG2,          THEME_INACTIVE    },
   { COLOR_TRAYBUTTON_ACTIVE_FG,    THEME_TEXT        },
   { COLOR_TRAYBUTTON_ACTIVE_BG1,   THEME_INACTIVE    },
   { COLOR_TRAYBUTTON_ACTIVE_BG2,   THEME_INACTIVE    },

   { COLOR_CLOCK_FG,                THEME_TEXT        },
   { COLOR_CLOCK_BG1,               THEME_INACTIVE    },
   { COLOR_CLOCK_BG2,               THEME_INACTIVE    },

   { COLOR_PAGER_FG,                THEME_INACTIVE    },
   { COLOR_PAGER_BG,                THEME_INACTIVE2   },
   { COLOR_PAGER_ACTIVE_BG,         THEME_ACTIVE2     },
   { COLOR_PAGER_ACTIVE_FG,         THEME_ACTIVE      },
   { COLOR_PAGER_OUTLINE,           THEME_OUTLINE     },
   { COLOR_PAGER_TEXT,              THEME_TEXT        },

   { COLOR_MENU_FG,                 THEME_TEXT        },
   { COLOR_MENU_BG,                 THEME_INACTIVE    },
   { COLOR_MENU_ACTIVE_FG,          THEME_TEXT        },
   { COLOR_MENU_ACTIVE_BG1,         THEME_ACTIVE      },
   { COLOR_MENU_ACTIVE_BG2,         THEME_ACTIVE      },

   { COLOR_POPUP_BG,                THEME_INACTIVE2   },
   { COLOR_POPUP_FG,                THEME_TEXT        },
   { COLOR_POPUP_OUTLINE,           THEME_OUTLINE     }

};
static const unsigned DEFAULT_COUNT = ARRAY_LENGTH(DEFAULT_COLORS);

static struct {
   ColorType src;
   ColorType dest;
} INHERITED_COLORS[] = {
   { COLOR_TRAY_FG,           COLOR_CLOCK_FG                },
   { COLOR_TRAY_BG1,          COLOR_CLOCK_BG1               },
   { COLOR_TRAY_BG2,          COLOR_CLOCK_BG2               },
   { COLOR_TRAY_FG,           COLOR_TASKLIST_FG             },
   { COLOR_TRAY_BG1,          COLOR_TASKLIST_BG1            },
   { COLOR_TRAY_BG2,          COLOR_TASKLIST_BG2            },
   { COLOR_TRAY_ACTIVE_FG,    COLOR_TASKLIST_ACTIVE_FG      },
   { COLOR_TRAY_ACTIVE_BG1,   COLOR_TASKLIST_ACTIVE_BG1     },
   { COLOR_TRAY_ACTIVE_BG2,   COLOR_TASKLIST_ACTIVE_BG2     },
   { COLOR_TRAY_UP,           COLOR_TASKLIST_UP             },
   { COLOR_TRAY_DOWN,         COLOR_TASKLIST_DOWN           },
   { COLOR_TRAY_ACTIVE_UP,    COLOR_TASKLIST_ACTIVE_UP      },
   { COLOR_TRAY_ACTIVE_DOWN,  COLOR_TASKLIST_ACTIVE_DOWN    },
   { COLOR_TRAY_FG,           COLOR_TRAYBUTTON_FG           },
   { COLOR_TRAY_BG1,          COLOR_TRAYBUTTON_BG1          },
   { COLOR_TRAY_BG2,          COLOR_TRAYBUTTON_BG2          },
   { COLOR_TRAY_ACTIVE_FG,    COLOR_TRAYBUTTON_ACTIVE_FG    },
   { COLOR_TRAY_ACTIVE_BG1,   COLOR_TRAYBUTTON_ACTIVE_BG1   },
   { COLOR_TRAY_ACTIVE_BG2,   COLOR_TRAYBUTTON_ACTIVE_BG2   },
   { COLOR_TRAY_UP,           COLOR_TRAYBUTTON_UP           },
   { COLOR_TRAY_DOWN,         COLOR_TRAYBUTTON_DOWN         },
   { COLOR_TRAY_ACTIVE_UP,    COLOR_TRAYBUTTON_ACTIVE_UP    },
   { COLOR_TRAY_ACTIVE_DOWN,  COLOR_TRAYBUTTON_ACTIVE_DOWN  },
   { COLOR_TASKLIST_FG,       COLOR_TASKLIST_MINIMIZED_FG   },
   { COLOR_TASKLIST_BG1,      COLOR_TASKLIST_MINIMIZED_BG1  },
   { COLOR_TASKLIST_BG2,      COLOR_TASKLIST_MINIMIZED_BG2  },
   { COLOR_TASKLIST_UP,       COLOR_TASKLIST_MINIMIZED_UP   },
   { COLOR_TASKLIST_DOWN,     COLOR_TASKLIST_MINIMIZED_DOWN }
};
static const unsigned INHERITED_COUNT = ARRAY_LENGTH(INHERITED_COLORS);

static struct {
   ColorType base;
   ColorType up;
   ColorType down;
} DERIVED_COLORS[] = {
   { COLOR_TITLE_BG1,        COLOR_TITLE_UP,        COLOR_TITLE_DOWN        },
   { COLOR_TITLE_ACTIVE_BG1, COLOR_TITLE_ACTIVE_UP, COLOR_TITLE_ACTIVE_DOWN },
   { COLOR_TRAY_BG1,         COLOR_TRAY_UP,         COLOR_TRAY_DOWN         },
   { COLOR_TRAY_ACTIVE_BG1,  COLOR_TRAY_ACTIVE_UP,  COLOR_TRAY_ACTIVE_DOWN  },
   { COLOR_TASKLIST_BG1,     COLOR_TASKLIST_UP,     COLOR_TASKLIST_DOWN     },
   { COLOR_TASKLIST_ACTIVE_BG1,
        COLOR_TASKLIST_ACTIVE_UP,   COLOR_TASKLIST_ACTIVE_DOWN              },
   { COLOR_TASKLIST_MINIMIZED_BG1,
        COLOR_TASKLIST_MINIMIZED_UP,   COLOR_TASKLIST_MINIMIZED_DOWN        },
   { COLOR_TRAYBUTTON_BG1,   COLOR_TRAYBUTTON_UP,   COLOR_TRAYBUTTON_DOWN   },
   { COLOR_TRAYBUTTON_ACTIVE_BG1,
        COLOR_TRAYBUTTON_ACTIVE_UP, COLOR_TRAYBUTTON_ACTIVE_DOWN            },
   { COLOR_MENU_BG,          COLOR_MENU_UP,         COLOR_MENU_DOWN         },
   { COLOR_MENU_ACTIVE_BG1,  COLOR_MENU_ACTIVE_UP,  COLOR_MENU_ACTIVE_DOWN  }
};
static const unsigned DERIVED_COUNT = ARRAY_LENGTH(DERIVED_COLORS);

static char **names = NULL;

static unsigned redShift;
static unsigned greenShift;
static unsigned blueShift;
static unsigned redBits;
static unsigned greenBits;
static unsigned blueBits;
static unsigned long alphaMask;

static unsigned ComputeShift(unsigned long maskIn, unsigned *shiftOut);
static unsigned long GetRGBFromXColor(const XColor *c);

static unsigned long GetDirectPixel(const XColor *c);
static void GetMappedPixel(XColor *c);
static void AllocateColor(ColorType type, XColor *c);

static unsigned long ReadHex(const char *hex);
char ParseColorToRGB(const char *value, XColor *c);

static void InitializeNames(void);

static XColor GetXColorFromRGB(unsigned long rgb);
static void LightenColor(ColorType oldColor, ColorType newColor);
static void DarkenColor(ColorType oldColor, ColorType newColor);

/** Startup color support. */
void StartupColors(void)
{
   unsigned int x;
   XColor c;

   /* Initialize the color array. */
   memset(rgbColors, 0xFF, sizeof(rgbColors));

   /* Determine how to convert between RGB triples and pixels. */
   switch(rootVisual->class) {
   case DirectColor:
   case TrueColor:
      redBits = ComputeShift(rootVisual->red_mask, &redShift);
      greenBits = ComputeShift(rootVisual->green_mask, &greenShift);
      blueBits = ComputeShift(rootVisual->blue_mask, &blueShift);
      alphaMask = (~0UL >> (8 * sizeof(unsigned long) - rootDepth))
                & ~(((1UL << redBits  ) - 1) << redShift)
                & ~(((1UL << greenBits) - 1) << greenShift)
                & ~(((1UL << blueBits ) - 1) << blueShift);
      rgbToPixel = NULL;
      break;
   default:
      /* Restrict icons to 64 colors (RGB 2, 2, 2). */
      redBits = ComputeShift(0x30, &redShift);
      greenBits = ComputeShift(0x0C, &greenShift);
      blueBits = ComputeShift(0x03, &blueShift);
      alphaMask = 0;
      rgbToPixel = Allocate(sizeof(unsigned long) * MAX_COLORS);
      memset(rgbToPixel, 0xFF, sizeof(unsigned long) * MAX_COLORS);
      break;
   }

   /* Inherit colors. */
   if(names) {
      for(x = 0; x < INHERITED_COUNT; x++) {
         const ColorType dest = INHERITED_COLORS[x].dest;
         if(!names[dest]) {
            const ColorType src = INHERITED_COLORS[x].src;
            names[dest] = CopyString(names[src]);
            gradients[dest] = gradients[src];
         }
      }
   }

   /* Load custom colors. */
   for(x = 0; x < COLOR_COUNT; x++) {
      if(names && names[x]) {
         if(ParseColorToRGB(names[x], &c)) {
            AllocateColor(x, &c);
         } else {
            Release(names[x]);
            names[x] = NULL;
         }
      }
   }

   /* Load defaults. */
   for(x = 0; x < DEFAULT_COUNT; x++) {
      const ColorType type = DEFAULT_COLORS[x].type;
      if(!names || !names[type]) {
         const unsigned rgb = DEFAULT_COLORS[x].value;
         c.red = ((rgb >> 16) & 0xFF) * 257;
         c.green = ((rgb >> 8) & 0xFF) * 257;
         c.blue = (rgb & 0xFF) * 257;
         AllocateColor(type, &c);
      }
   }

   /* Derive colors. */
   for(x = 0; x < DERIVED_COUNT; x++) {
      if(!names || !names[DERIVED_COLORS[x].up]) {
         LightenColor(DERIVED_COLORS[x].base, DERIVED_COLORS[x].up);
      }
      if(!names || !names[DERIVED_COLORS[x].down]) {
         DarkenColor(DERIVED_COLORS[x].base, DERIVED_COLORS[x].down);
      }
   }

   DestroyColors();
}

/** Shutdown color support. */
void ShutdownColors(void)
{
   unsigned x;

#ifdef USE_XFT
   for(x = 0; x < COLOR_COUNT; x++) {
      if(xftColors[x]) {
         JXftColorFree(display, rootVisual, rootColormap, xftColors[x]);
         Release(xftColors[x]);
         xftColors[x] = NULL;
      }
   }
#endif

   if(rgbToPixel) {
      for(x = 0; x < MAX_COLORS; x++) {
         if(rgbToPixel[x] != ULONG_MAX) {
            JXFreeColors(display, rootColormap, &rgbToPixel[x], 1, 0);
         }
      }
      for(x = 0; x < COLOR_COUNT; x++) {
         if(rgbColors[x] != ULONG_MAX) {
            unsigned y;
            JXFreeColors(display, rootColormap, &colors[x], 1, 0);
            for(y = x; y < COLOR_COUNT; y++) {
               if(colors[y] == colors[x]) {
                  rgbColors[y] = ULONG_MAX;
               }
            }
         }
      }
      Release(rgbToPixel);
      rgbToPixel = NULL;
   }

}

/** Release color data. */
void DestroyColors(void)
{
   if(names) {
      unsigned int x;
      for(x = 0; x < COLOR_COUNT; x++) {
         if(names[x]) {
            Release(names[x]);
         }
      }
      Release(names);
      names = NULL;
   }
}

/** Compute the shift and bits for a linear RGB colormap. */
unsigned ComputeShift(unsigned long maskIn, unsigned *shiftOut)
{
   unsigned bits = 0;
   unsigned shift = 0;
   while(maskIn && (maskIn & 1) == 0) {
      shift += 1;
      maskIn >>= 1;
   }
   *shiftOut = shift;
   while(maskIn) {
      bits += 1;
      maskIn >>= 1;
   }
   return bits;
}

/** Get an RGB value from an XColor. */
unsigned long GetRGBFromXColor(const XColor *c)
{
   unsigned long red, green, blue;
   red   = (c->red   >> 8) << 16;
   green = (c->green >> 8) << 8;
   blue  = (c->blue  >> 8) << 0;
   return red | green | blue;
}

/** Set the color to use for a component. */
void SetColor(ColorType c, const char *value)
{
   if(JUNLIKELY(!value)) {
      Warning("empty color tag");
      return;
   }

   InitializeNames();

   if(names[c]) {
      Release(names[c]);
   }

   names[c] = CopyString(value);
}

/** Set the gradient direction to use for a pair of colors. */
void SetGradient(ColorType a, ColorType b, GradientDirection d)
{
   gradients[a] = d;
   gradients[b] = d;
}

/** Parse a color without lookup. */
char ParseColorToRGB(const char *value, XColor *c)
{
   if(JUNLIKELY(!value)) {
      return 0;
   }

   if(value[0] == '#') {
      const unsigned len = strlen(value);
      const unsigned long rgb = ReadHex(value + 1);
      if(len == 4) {
         c->red = ((rgb >> 8) & 0x0F) * 4369;
         c->green = ((rgb >> 4) & 0x0F) * 4369;
         c->blue = (rgb & 0x0F) * 4369;
         return 1;
      } else if(len == 7) {
         const unsigned long rgb = ReadHex(value + 1);
         c->red = ((rgb >> 16) & 0xFF) * 257;
         c->green = ((rgb >> 8) & 0xFF) * 257;
         c->blue = (rgb & 0xFF) * 257;
         return 1;
      }
   }

   if(JXParseColor(display, rootColormap, value, c)) {
      return 1;
   }

   Warning("bad color: \"%s\"", value);
   return 0;
}

/** Parse a color for a component. */
char ParseColor(const char *value, XColor *c)
{
   if(JLIKELY(ParseColorToRGB(value, c))) {
      GetColor(c);
      return 1;
   } else {
      return 0;
   }
}

/** Initialize color names to NULL. */
void InitializeNames(void)
{
   if(names == NULL) {
      unsigned int x;
      names = Allocate(sizeof(char*) * COLOR_COUNT);
      for(x = 0; x < COLOR_COUNT; x++) {
         names[x] = NULL;
      }
   }
}

/** Convert a hex value to an unsigned long. */
unsigned long ReadHex(const char *hex)
{
   unsigned long value = 0;
   unsigned int x;

   for(x = 0; hex[x]; x++) {
      value *= 16;
      if(hex[x] >= '0' && hex[x] <= '9') {
         value += hex[x] - '0';
      } else if(hex[x] >= 'A' && hex[x] <= 'F') {
         value += hex[x] - 'A' + 10;
      } else if(hex[x] >= 'a' && hex[x] <= 'f') {
         value += hex[x] - 'a' + 10;
      }
   }

   return value;
}

/** Compute the pixel value from RGB components. */
unsigned long GetDirectPixel(const XColor *c)
{
   const unsigned long red   = (c->red   >> (16 - redBits  )) << redShift;
   const unsigned long green = (c->green >> (16 - greenBits)) << greenShift;
   const unsigned long blue  = (c->blue  >> (16 - blueBits )) << blueShift;
   return red | green | blue | alphaMask;
}

/** Compute the pixel value from RGB components. */
void GetMappedPixel(XColor *c)
{
   const unsigned long index = GetDirectPixel(c);
   if(rgbToPixel[index] == ULONG_MAX) {
      c->red   = (c->red   & 0xC000) | 0x0800;
      c->green = (c->green & 0xC000) | 0x0800;
      c->blue  = (c->blue  & 0xC000) | 0x0800;
      JXAllocColor(display, rootColormap, c);
      rgbToPixel[index] = c->pixel;
   } else {
      c->pixel = rgbToPixel[index];
   }
}

/** Allocate a pixel from RGB components. */
void AllocateColor(ColorType type, XColor *c)
{
   unsigned i;

   /* Save the desired RGB color. */
   rgbColors[type] = GetRGBFromXColor(c);

   /* Look up the pixel value to use. */
   switch(rootVisual->class) {
   case DirectColor:
   case TrueColor:
      c->pixel = GetDirectPixel(c);
      break;
   default:
      /* First see if we already know about this color. */
      for(i = 0; i < COLOR_COUNT; i++) {
         if(i != type && rgbColors[i] == rgbColors[type]) {
            colors[type] = colors[i];
            return;
         }
      }

      /* Allocate a new read-only cell. */
      JXAllocColor(display, rootColormap, c);
      break;
   }
   colors[type] = c->pixel;
}

/** Compute the pixel value from RGB components. */
void GetColor(XColor *c)
{
   switch(rootVisual->class) {
   case DirectColor:
   case TrueColor:
      c->pixel = GetDirectPixel(c);
      return;
   default:
      GetMappedPixel(c);
      return;
   }
}

/** Get an XFT color for the specified component. */
#ifdef USE_XFT
XftColor *GetXftColor(ColorType type)
{

   if(!xftColors[type]) {
      XRenderColor rcolor;
      const unsigned long rgb = rgbColors[type];
      xftColors[type] = Allocate(sizeof(XftColor));
      rcolor.alpha = 65535;
      rcolor.red = ((rgb >> 16) & 0xFF) * 257;
      rcolor.green = ((rgb >> 8) & 0xFF) * 257;
      rcolor.blue = (rgb & 0xFF) * 257;
      JXftColorAllocValue(display, rootVisual, rootColormap, &rcolor,
                          xftColors[type]);
   }

   return xftColors[type];

}
#endif

/** Convert an RGB value to an XColor. */
XColor GetXColorFromRGB(unsigned long rgb)
{
   XColor ret = { 0 };

   ret.flags = DoRed | DoGreen | DoBlue;
   ret.red = (unsigned short)(((rgb >> 16) & 0xFF) * 257);
   ret.green = (unsigned short)(((rgb >> 8) & 0xFF) * 257);
   ret.blue = (unsigned short)((rgb & 0xFF) * 257);

   return ret;
}

/** Compute a color lighter than the input. */
void LightenColor(ColorType oldColor, ColorType newColor)
{

   XColor temp;
   int red, green, blue;

   temp = GetXColorFromRGB(rgbColors[oldColor]);

   /* Convert to 0.0 to 1.0 in fixed point with 8 bits for the fraction. */
   red   = temp.red   >> 8;
   green = temp.green >> 8;
   blue  = temp.blue  >> 8;

   /* Multiply by 1.45 which is 371. */
   red   = (red   * 371) >> 8;
   green = (green * 371) >> 8;
   blue  = (blue  * 371) >> 8;

   /* Convert back to 0-65535. */
   red   |= red << 8;
   green |= green << 8;
   blue  |= blue << 8;

   /* Cap at 65535. */
   red   = Min(65535, red);
   green = Min(65535, green);
   blue  = Min(65535, blue);

   temp.red = red;
   temp.green = green;
   temp.blue = blue;

   AllocateColor(newColor, &temp);
}

/** Compute a color darker than the input. */
void DarkenColor(ColorType oldColor, ColorType newColor)
{

   XColor temp;
   int red, green, blue;

   temp = GetXColorFromRGB(rgbColors[oldColor]);

   /* Convert to 0.0 to 1.0 in fixed point with 8 bits for the fraction. */
   red   = temp.red   >> 8;
   green = temp.green >> 8;
   blue  = temp.blue  >> 8;

   /* Multiply by 0.55 which is 141. */
   red   = (red   * 141) >> 8;
   green = (green * 141) >> 8;
   blue  = (blue  * 141) >> 8;

   /* Convert back to 0-65535. */
   red   |= red << 8;
   green |= green << 8;
   blue  |= blue << 8;

   temp.red = red;
   temp.green = green;
   temp.blue = blue;

   AllocateColor(newColor, &temp);
}
