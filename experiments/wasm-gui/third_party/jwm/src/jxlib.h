/**
 * @file jxlib.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Macros to wrap X calls for debugging and testing.
 *
 */

#ifndef JXLIB_H
#define JXLIB_H

#ifdef UNIT_TEST

#  define JFUNC1(name, a) Mock_##name(a)
#  define JFUNC2(name, a, b) Mock_##name(a, b)
#  define JFUNC3(name, a, b, c) Mock_##name(a, b, c)
#  define JFUNC4(name, a, b, c, d) Mock_##name(a, b, c, d)
#  define JFUNC5(name, a, b, c, d, e) Mock_##name(a, b, c, d, e)
#  define JFUNC6(name, a, b, c, d, e, f) Mock_##name(a, b, c, d, e, f)
#  define JFUNC7(name, a, b, c, d, e, f, g) Mock_##name(a, b, c, d, e, f, g)
#  define JFUNC8(name, a, b, c, d, e, f, g, h) \
   Mock_##name(a, b, c, d, e, f, g, h)
#  define JFUNC9(name, a, b, c, d, e, f, g, h, i) \
   Mock_##name(a, b, c, d, e, f, g, h, i)
#  define JFUNC10(name, a, b, c, d, e, f, g, h, i, j) \
   Mock_##name(a, b, c, d, e, f, g, h, i, j)
#  define JFUNC11(name, a, b, c, d, e, f, g, h, i, j, k) \
   Mock_##name(a, b, c, d, e, f, g, h, i, j, k)
#  define JFUNC12(name, a, b, c, d, e, f, g, h, i, j, k, l) \
   Mock_##name(a, b, c, d, e, f, g, h, i, j, k, l)
#  define JFUNC13(name, a, b, c, d, e, f, g, h, i, j, k, l, m) \
   Mock_##name(a, b, c, d, e, f, g, h, i, j, k, l, m)

#else

#  define JFUNC1(name, a) (SetCheckpoint(), name(a))
#  define JFUNC2(name, a, b) (SetCheckpoint(), name(a, b))
#  define JFUNC3(name, a, b, c) (SetCheckpoint(), name(a, b, c))
#  define JFUNC4(name, a, b, c, d) (SetCheckpoint(), name(a, b, c, d))
#  define JFUNC5(name, a, b, c, d, e) (SetCheckpoint(), name(a, b, c, d, e))
#  define JFUNC6(name, a, b, c, d, e, f) \
   (SetCheckpoint(), name(a, b, c, d, e, f))
#  define JFUNC7(name, a, b, c, d, e, f, g) \
   (SetCheckpoint(), name(a, b, c, d, e, f, g))
#  define JFUNC8(name, a, b, c, d, e, f, g, h) \
   (SetCheckpoint(), name(a, b, c, d, e, f, g, h))
#  define JFUNC9(name, a, b, c, d, e, f, g, h, i) \
   (SetCheckpoint(), name(a, b, c, d, e, f, g, h, i))
#  define JFUNC10(name, a, b, c, d, e, f, g, h, i, j) \
   (SetCheckpoint(), name(a, b, c, d, e, f, g, h, i, j))
#  define JFUNC11(name, a, b, c, d, e, f, g, h, i, j, k) \
   (SetCheckpoint(), name(a, b, c, d, e, f, g, h, i, j, k))
#  define JFUNC12(name, a, b, c, d, e, f, g, h, i, j, k, l) \
   (SetCheckpoint(), name(a, b, c, d, e, f, g, h, i, j, k, l))
#  define JFUNC13(name, a, b, c, d, e, f, g, h, i, j, k, l, m) \
   (SetCheckpoint(), name(a, b, c, d, e, f, g, h, i, j, k, l, m))

#endif

#define JXAddToSaveSet( a, b ) JFUNC2(XAddToSaveSet, a, b)

#define JXAllocColor( a, b, c ) JFUNC3(XAllocColor, a, b, c)

#define JXGetRGBColormaps( a, b, c, d, e ) \
   JFUNC5(XGetRGBColormaps, a, b, c, d, e)

#define JXQueryColor( a, b, c ) JFUNC3(XQueryColor, a, b, c)

#define JXQueryColors( a, b, c, d ) JFUNC4(XQueryColors, a, b, c, d)

#define JXAllowEvents( a, b, c ) JFUNC3(XAllowEvents, a, b, c)

#define JXChangeProperty( a, b, c, d, e, f, g, h ) \
   JFUNC8(XChangeProperty, a, b, c, d, e, f, g, h)

#define JXDeleteProperty( a, b, c ) JFUNC3(XDeleteProperty, a, b, c)

#define JXChangeWindowAttributes( a, b, c, d ) \
   JFUNC4(XChangeWindowAttributes, a, b, c, d)

#define JXCheckTypedEvent( a, b, c ) JFUNC3(XCheckTypedEvent, a, b, c)

#define JXCheckTypedWindowEvent( a, b, c, d ) \
   JFUNC4(XCheckTypedWindowEvent, a, b, c, d)

#define JXClearWindow( a, b ) JFUNC2(XClearWindow, a, b)

#define JXClearArea( a, b, c, d, e, f, g ) \
   JFUNC7(XClearArea, a, b, c, d, e, f, g)

#define JXCloseDisplay( a ) JFUNC1(XCloseDisplay, a)

#define JXConfigureWindow( a, b, c, d ) JFUNC4(XConfigureWindow, a, b, c, d)

#define JXConnectionNumber( a ) JFUNC1(XConnectionNumber, a)

#define JXCopyArea( a, b, c, d, e, f, g, h, i, j ) \
   JFUNC10(XCopyArea, a, b, c, d, e, f, g, h, i, j)

#define JXCopyPlane( a, b, c, d, e, f, g, h, i, j, k ) \
   JFUNC11(XCopyPlane, a, b, c, d, e, f, g, h, i, j, k)

#define JXCreateFontCursor( a, b ) JFUNC2(XCreateFontCursor, a, b)

#define JXCreateGC( a, b, c, d ) JFUNC4(XCreateGC, a, b, c, d)

#define JXCreateImage( a, b, c, d, e, f, g, h, i, j ) \
   JFUNC10(XCreateImage, a, b, c, d, e, f, g, h, i, j)

#define JXCreatePixmap( a, b, c, d, e ) \
   JFUNC5(XCreatePixmap, a, b, c, d, e)

#define JXCreatePixmapFromBitmapData( a, b, c, d, e, f, g, h ) \
   JFUNC8(XCreatePixmapFromBitmapData, a, b, c, d, e, f, g, h)

#define JXCreateBitmapFromData( a, b, c, d, e ) \
   JFUNC5(XCreateBitmapFromData, a, b, c, d, e)

#define JXCreateSimpleWindow( a, b, c, d, e, f, g, h, i ) \
   JFUNC9(XCreateSimpleWindow, a, b, c, d, e, f, g, h, i)

#define JXCreateWindow( a, b, c, d, e, f, g, h, i, j, k, l ) \
   JFUNC12(XCreateWindow, a, b, c, d, e, f, g, h, i, j, k, l)

#define JXDefineCursor( a, b, c ) JFUNC3(XDefineCursor, a, b, c)

#define JXDestroyImage( a ) JFUNC1(XDestroyImage, a)

#define JXDestroyWindow( a, b ) JFUNC2(XDestroyWindow, a, b)

#define JXDrawPoint( a, b, c, d, e ) JFUNC5(XDrawPoint, a, b, c, d, e)

#define JXDrawPoints( a, b, c, d, e, f ) \
   JFUNC6(XDrawPoints, a, b, c, d, e, f)

#define JXDrawLine( a, b, c, d, e, f, g ) \
   JFUNC7(XDrawLine, a, b, c, d, e, f, g)

#define JXDrawSegments( a, b, c, d, e ) \
   JFUNC5(XDrawSegments, a, b, c, d, e)

#define JXDrawRectangle( a, b, c, d, e, f, g ) \
   JFUNC7(XDrawRectangle, a, b, c, d, e, f, g)

#define JXFillRectangles( a, b, c, d, e ) \
   JFUNC5(XFillRectangles, a, b, c, d, e)

#define JXDrawArcs( a, b, c, d, e ) JFUNC5(XDrawArcs, a, b, c, d, e)

#define JXFillArcs( a, b, c, d, e ) JFUNC5(XFillArcs, a, b, c, d, e)

#define JXSetLineAttributes( a, b, c, d, e, f ) \
   JFUNC6(XSetLineAttributes, a, b, c, d, e, f)

#define JXDrawString( a, b, c, d, e, f, g ) \
   JFUNC7(XDrawString, a, b, c, d, e, f, g)

#define JXFetchName( a, b, c ) JFUNC3(XFetchName, a, b, c)

#define JXFillRectangle( a, b, c, d, e, f, g ) \
   JFUNC7(XFillRectangle, a, b, c, d, e, f, g)

#define JXFlush( a ) JFUNC1(XFlush, a)

#define JXFree( a ) JFUNC1(XFree, a)

#define JXFreeColors( a, b, c, d, e ) JFUNC5(XFreeColors, a, b, c, d, e)

#define JXFreeCursor( a, b ) JFUNC2(XFreeCursor, a, b)

#define JXFreeFont( a, b ) JFUNC2(XFreeFont, a, b)

#define JXFreeGC( a, b ) JFUNC2(XFreeGC, a, b)

#define JXFreeModifiermap( a ) JFUNC1(XFreeModifiermap, a)

#define JXFreePixmap( a, b ) JFUNC2(XFreePixmap, a, b)

#define JXGetAtomName( a, b ) JFUNC2(XGetAtomName, a, b)

#define JXGetModifierMapping( a ) JFUNC1(XGetModifierMapping, a)

#define JXGetSubImage( a, b, c, d, e, f, g, h, i, j, k ) \
   JFUNC11(XGetSubImage, a, b, c, d, e, f, g, h, i, j, k)

#define JXGetTransientForHint( a, b, c ) JFUNC3(XGetTransientForHint, a, b, c)

#define JXGetClassHint( a, b, c ) JFUNC3(XGetClassHint, a, b, c)

#define JXGetWindowAttributes( a, b, c ) JFUNC3(XGetWindowAttributes, a, b, c)

#define JXGetWindowProperty( a, b, c, d, e, f, g, h, i, j, k, l ) \
   JFUNC12(XGetWindowProperty, a, b, c, d, e, f, g, h, i, j, k, l)

#define JXGetWMColormapWindows( a, b, c, d ) \
   JFUNC4(XGetWMColormapWindows, a, b, c, d)

#define JXGetWMNormalHints( a, b, c, d ) JFUNC4(XGetWMNormalHints, a, b, c, d)

#define JXSetIconSizes( a, b, c, d ) JFUNC4(XSetIconSizes, a, b, c, d)

#define JXSetWindowBorder( a, b, c ) JFUNC3(XSetWindowBorder, a, b, c)

#define JXGetWMHints( a, b ) JFUNC2(XGetWMHints, a, b)

#define JXGrabButton( a, b, c, d, e, f, g, h, i, j ) \
   JFUNC10(XGrabButton, a, b, c, d, e, f, g, h, i, j)

#define JXKeycodeToKeysym( a, b, c ) JFUNC3(XKeycodeToKeysym, a, b, c)

#define JXGrabKey( a, b, c, d, e, f, g ) \
   JFUNC7(XGrabKey, a, b, c, d, e, f, g)

#define JXUngrabKey( a, b, c, d ) JFUNC4(XUngrabKey, a, b, c, d)

#define JXGrabKeyboard( a, b, c, d, e, f ) \
   ( SetCheckpoint(), XGrabKeyboard( a, b, c, d, e, f ) )

#define JXGrabPointer( a, b, c, d, e, f, g, h, i ) \
   ( SetCheckpoint(), XGrabPointer( a, b, c, d, e, f, g, h, i ) )

#define JXGrabServer( a ) JFUNC1(XGrabServer, a)

#define JXInstallColormap( a, b ) JFUNC2(XInstallColormap, a, b)

#define JXInternAtom( a, b, c ) JFUNC3(XInternAtom, a, b, c)

#define JXKeysymToKeycode( a, b ) JFUNC2(XKeysymToKeycode, a, b)

#define JXKillClient( a, b ) JFUNC2(XKillClient, a, b)

#define JXLoadQueryFont( a, b ) JFUNC2(XLoadQueryFont, a, b)

#define JXMapRaised( a, b ) JFUNC2(XMapRaised, a, b)

#define JXMapWindow( a, b ) JFUNC2(XMapWindow, a, b)

#define JXMoveResizeWindow( a, b, c, d, e, f ) \
   JFUNC6(XMoveResizeWindow, a, b, c, d, e, f)

#define JXMoveWindow( a, b, c, d ) JFUNC4(XMoveWindow, a, b, c, d)

#define JXNextEvent( a, b ) JFUNC2(XNextEvent, a, b)

#define JXMaskEvent( a, b, c ) JFUNC3(XMaskEvent, a, b, c)

#define JXCheckMaskEvent( a, b, c ) JFUNC3(XCheckMaskEvent, a, b, c)

#define JXOpenDisplay( a ) JFUNC1(XOpenDisplay, a)

#define JXParseColor( a, b, c, d ) JFUNC4(XParseColor, a, b, c, d)

#define JXPending( a ) JFUNC1(XPending, a)

#define JXPutBackEvent( a, b ) JFUNC2(XPutBackEvent, a, b)

#define JXGetImage( a, b, c, d, e, f, g, h ) \
   JFUNC8(XGetImage, a, b, c, d, e, f, g, h)

#define JXPutImage( a, b, c, d, e, f, g, h, i, j ) \
   JFUNC10(XPutImage, a, b, c, d, e, f, g, h, i, j)

#define JXQueryPointer( a, b, c, d, e, f, g, h, i ) \
   JFUNC9(XQueryPointer, a, b, c, d, e, f, g, h, i)

#define JXQueryTree( a, b, c, d, e, f ) JFUNC6(XQueryTree, a, b, c, d, e, f)

#define JXReparentWindow( a, b, c, d, e ) \
   JFUNC5(XReparentWindow, a, b, c, d, e)

#define JXRemoveFromSaveSet( a, b ) JFUNC2(XRemoveFromSaveSet, a, b)

#define JXResizeWindow( a, b, c, d ) JFUNC4(XResizeWindow, a, b, c, d)

#define JXRestackWindows( a, b, c ) JFUNC3(XRestackWindows, a, b, c)

#define JXRaiseWindow( a, b ) JFUNC2(XRaiseWindow, a, b)

#define JXSelectInput( a, b, c ) JFUNC3(XSelectInput, a, b, c)

#define JXSendEvent( a, b, c, d, e ) JFUNC5(XSendEvent, a, b, c, d, e)

#define JXSetBackground( a, b, c ) JFUNC3(XSetBackground, a, b, c)

#define JXSetClipMask( a, b, c ) JFUNC3(XSetClipMask, a, b, c)

#define JXSetClipOrigin( a, b, c, d ) JFUNC4(XSetClipOrigin, a, b, c, d)

#define JXSetClipRectangles( a, b, c, d, e, f, g ) \
   JFUNC7(XSetClipRectangles, a, b, c, d, e, f, g)

#define JXSetErrorHandler( a ) JFUNC1(XSetErrorHandler, a)

#define JXSetFont( a, b, c ) JFUNC3(XSetFont, a, b, c)

#define JXSetForeground( a, b, c ) JFUNC3(XSetForeground, a, b, c)

#define JXGetInputFocus( a, b, c ) JFUNC3(XGetInputFocus, a, b, c)

#define JXSetInputFocus( a, b, c, d ) JFUNC4(XSetInputFocus, a, b, c, d)

#define JXSetWindowBackground( a, b, c ) JFUNC3(XSetWindowBackground, a, b, c)

#define JXSetWindowBackgroundPixmap( a, b, c ) JFUNC3(XSetWindowBackgroundPixmap, a, b, c)

#define JXSetWindowBorderWidth( a, b, c ) \
   JFUNC3(XSetWindowBorderWidth, a, b, c)

#define JXSetWMNormalHints( a, b, c ) JFUNC3(XSetWMNormalHints, a, b, c)

#define JXShapeCombineRectangles( a, b, c, d, e, f, g, h, i ) \
   JFUNC9(XShapeCombineRectangles, a, b, c, d, e, f, g, h, i)

#define JXShapeCombineShape( a, b, c, d, e, f, g, h ) \
   JFUNC8(XShapeCombineShape, a, b, c, d, e, f, g, h)

#define JXShapeCombineMask( a, b, c, d, e, f, g ) \
   JFUNC7(XShapeCombineMask, a, b, c, d, e, f, g)

#define JXShapeQueryExtension( a, b, c ) \
   JFUNC3(XShapeQueryExtension, a, b, c)

#define JXQueryExtension( a, b, c, d, e ) \
   JFUNC5(XQueryExtension, a, b, c, d, e)

#define JXShapeQueryExtents( a, b, c, d, e, f, g, h, i, j, k, l ) \
   JFUNC12(XShapeQueryExtents, a, b, c, d, e, f, g, h, i, j, k, l)

#define JXShapeGetRectangles( a, b, c, d, e ) \
   JFUNC5(XShapeGetRectangles, a, b, c, d, e)

#define JXShapeSelectInput( a, b, c ) JFUNC3(XShapeSelectInput, a, b, c)

#define JXStoreName( a, b, c ) JFUNC3(XStoreName, a, b, c)

#define JXStringToKeysym( a ) JFUNC1(XStringToKeysym, a)

#define JXSync( a, b ) JFUNC2(XSync, a, b)

#define JXTextWidth( a, b, c ) JFUNC3(XTextWidth, a, b, c)

#define JXUngrabButton( a, b, c, d ) JFUNC4(XUngrabButton, a, b, c, d)

#define JXUngrabKeyboard( a, b ) JFUNC2(XUngrabKeyboard, a, b)

#define JXUngrabPointer( a, b ) JFUNC2(XUngrabPointer, a, b)

#define JXUngrabServer( a ) JFUNC1(XUngrabServer, a)

#define JXUnmapWindow( a, b ) JFUNC2(XUnmapWindow, a, b)

#define JXWarpPointer( a, b, c, d, e, f, g, h, i ) \
   JFUNC9(XWarpPointer, a, b, c, d, e, f, g, h, i)

#define JXSetSelectionOwner( a, b, c, d ) \
   JFUNC4(XSetSelectionOwner, a, b, c, d)

#define JXGetSelectionOwner( a, b ) JFUNC2(XGetSelectionOwner, a, b)

#define JXSetRegion( a, b, c ) JFUNC3(XSetRegion, a, b, c)

#define JXGetGeometry( a, b, c, d, e, f, g, h, i ) \
   JFUNC9(XGetGeometry, a, b, c, d, e, f, g, h, i)

/* XFT */

#define JXftFontOpenName( a, b, c ) JFUNC3(XftFontOpenName, a, b, c)

#define JXftFontOpenXlfd( a, b, c ) JFUNC3(XftFontOpenXlfd, a, b, c)

#define JXftDrawCreate( a, b, c, d ) JFUNC4(XftDrawCreate, a, b, c, d)

#define JXftDrawDestroy( a ) JFUNC1(XftDrawDestroy, a)

#define JXftTextExtentsUtf8( a, b, c, d, e ) \
   JFUNC5(XftTextExtentsUtf8, a, b, c, d, e)

#define JXftDrawChange( a, b ) JFUNC2(XftDrawChange, a, b)

#define JXftDrawSetClipRectangles( a, b, c, d, e ) \
   JFUNC5(XftDrawSetClipRectangles, a, b, c, d, e)

#define JXftDrawStringUtf8( a, b, c, d, e, f, g ) \
   JFUNC7(XftDrawStringUtf8, a, b, c, d, e, f, g)

#define JXftColorFree( a, b, c, d ) JFUNC4(XftColorFree, a, b, c, d)

#define JXftColorAllocValue( a, b, c, d, e ) \
   JFUNC5(XftColorAllocValue, a, b, c, d, e)

#define JXftFontClose( a, b ) JFUNC2(XftFontClose, a, b)

#define JXftDrawSetClip( a, b ) JFUNC2(XftDrawSetClip, a, b)

/* Xrender */

#define JXRenderQueryExtension( a, b, c ) \
   JFUNC3(XRenderQueryExtension, a, b, c)

#define JXRenderFindVisualFormat( a, b ) \
   JFUNC2(XRenderFindVisualFormat, a, b)

#define JXRenderFindFormat( a, b, c, d ) \
   JFUNC4(XRenderFindFormat, a, b, c, d)

#define JXRenderFindStandardFormat( a, b ) \
   JFUNC2(XRenderFindStandardFormat, a, b)

#define JXRenderCreatePicture( a, b, c, d, e ) \
   JFUNC5(XRenderCreatePicture, a, b, c, d, e)

#define JXRenderFreePicture( a, b ) JFUNC2(XRenderFreePicture, a, b)

#define JXRenderComposite( a, b, c, d, e, f, g, h, i, j, k, l, m ) \
   JFUNC13(XRenderComposite, a, b, c, d, e, f, g, h, i, j, k, l, m)

#endif /* JXLIB_H */
