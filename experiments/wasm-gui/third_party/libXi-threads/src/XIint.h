/*
 *	XIint.h - Header definition and support file for the internal
 *	support routines used by the Xi library.
 */

#ifndef _XIINT_H_
#define _XIINT_H_
#include <X11/extensions/XI.h>

/* inputproto 2.0 still shipped with these defined in the proto headers */
#ifndef XInput_Initial_Release
/* Indices into the versions[] array (XExtInt.c). Used as a index to
 * retrieve the minimum version of XI from _XiCheckExtInit */
#define Dont_Check			0
#define XInput_Initial_Release		1
#define XInput_Add_XDeviceBell		2
#define XInput_Add_XSetDeviceValuators	3
#define XInput_Add_XChangeDeviceControl	4
#define XInput_Add_DevicePresenceNotify	5
#define XInput_Add_DeviceProperties	6
#define XInput_2_0			7
#endif
#define XInput_2_1			8
#define XInput_2_2			9
#define XInput_2_3			10
#define XInput_2_4			11

extern XExtDisplayInfo *XInput_find_display(Display *);

extern int _XiCheckExtInit(Display *, int, XExtDisplayInfo *);
extern int _XiCheckVersion(XExtDisplayInfo *info, int version_index);

extern XExtensionVersion* _XiGetExtensionVersionRequest(Display *dpy, _Xconst char *name, int xi_opcode);
extern Status _xiQueryVersion(Display *dpy, int*, int*, XExtDisplayInfo *);

extern Status _XiEventToWire(
    register Display *		/* dpy */,
    register XEvent *		/* re */,
    register xEvent **		/* event */,
    register int *		/* count */
);

typedef struct _XInputData
{
    XEvent data;
    XExtensionVersion *vers;
} XInputData;


/**
 * Returns the next valid memory block of the given size within the block
 * previously allocated.
 * Use letting pointers inside a struct point to bytes after the same
 * struct, e.g. during protocol parsing etc.
 *
 * Caller is responsible for allocating enough memory.
 *
 * Example:
 *    void *ptr;
 *    struct foo {
 *       int num_a;
 *       int num_b;
 *       int *a;
 *       int *b;
 *    } bar;
 *
 *    ptr = malloc(large_enough);
 *    bar = next_block(&ptr, sizeof(struct foo));
 *    bar->num_a = 10;
 *    bar->num_b = 20;
 *    bar->a = next_block(&ptr, bar->num_a);
 *    bar->b = next_block(&ptr, bar->num_b);
 */
static inline void*
next_block(void **ptr, int size) {
    void *ret = *ptr;

    if (!*ptr)
        return NULL;

    *(unsigned char**)ptr += size;

    return ret;
}


#endif
