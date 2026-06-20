/**
 * @file main.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Header for the main functions.
 *
 */

#ifndef MAIN_H
#define MAIN_H

extern Display *display;
extern Window rootWindow;
extern int rootWidth, rootHeight;
extern int rootScreen;
extern Colormap rootColormap;
extern Visual *rootVisual;
extern int rootDepth;
extern GC rootGC;
extern int colormapCount;
extern Window supportingWindow;
extern Atom managerSelection;

extern char *exitCommand;

extern unsigned int currentDesktop;
extern unsigned int previousDesktop;

extern char shouldExit;
extern char shouldRestart;
extern char isRestarting;
extern char shouldReload;
extern char initializing;

extern XContext clientContext;
extern XContext frameContext;

#ifdef USE_SHAPE
extern char haveShape;
extern int shapeEvent;
#endif
#ifdef USE_XRENDER
extern char haveRender;
#endif

extern char *configPath;

#endif /* MAIN_H */

