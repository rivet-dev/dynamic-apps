/*

Copyright 1993, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/

/*
 * Author: Ralph Mor, X Consortium
 */

#ifndef _SMLIBINT_H_
#define _SMLIBINT_H_

#include <X11/Xos.h>
#include <X11/Xfuncs.h>
#include <X11/Xmd.h>
#include <X11/ICE/ICEmsg.h>
#include <X11/ICE/ICEproto.h>
#include <X11/SM/SMproto.h>

#include <stdlib.h>

#ifndef NULL
#include <stddef.h>
#endif


/*
 * Vendor & Release
 */

#define SmVendorString	"MIT"
#define SmReleaseString	"1.0"


/*
 * Pad to a 64 bit boundary
 */

#define PAD64(_bytes) ((8 - ((unsigned int) (_bytes) % 8)) % 8)

#define PADDED_BYTES64(_bytes) (_bytes + PAD64 (_bytes))


/*
 * Pad to 32 bit boundary
 */

#define PAD32(_bytes) ((4 - ((unsigned int) (_bytes) % 4)) % 4)

#define PADDED_BYTES32(_bytes) (_bytes + PAD32 (_bytes))


/*
 * Number of 8 byte units in _bytes.
 */

#define WORD64COUNT(_bytes) (((unsigned int) ((_bytes) + 7)) >> 3)


/*
 * Compute the number of bytes for an ARRAY8 representation
 */

#define ARRAY8_BYTES(_len) (4 + _len + PAD64 (4 + _len))



/*
 * Byte swapping
 */

/* byte swap a long literal */
#define lswapl(_val) ((((_val) & 0xff) << 24) |\
		   (((_val) & 0xff00) << 8) |\
		   (((_val) & 0xff0000) >> 8) |\
		   (((_val) >> 24) & 0xff))

/* byte swap a short literal */
#define lswaps(_val) ((((_val) & 0xff) << 8) | (((_val) >> 8) & 0xff))


/*
 * STORE macros
 */

#define STORE_CARD32(_pBuf, _val) \
{ \
    *((CARD32 *) _pBuf) = _val; \
    _pBuf += 4; \
}


/*
 * EXTRACT macros
 */

#define EXTRACT_CARD16(_pBuf, _swap, _val) \
{ \
    _val = *((CARD16 *) _pBuf); \
    _pBuf += 2; \
    if (_swap) \
        _val = lswaps (_val); \
}

#define EXTRACT_CARD32(_pBuf, _swap, _val) \
{ \
    _val = *((CARD32 *) _pBuf); \
    _pBuf += 4; \
    if (_swap) \
        _val = lswapl (_val); \
}


/*
 * Compute the number of bytes for a LISTofPROPERTY representation
 */

#define LISTOF_PROP_BYTES(_numProps, _props, _bytes) \
{ \
    int _i, _j; \
    _bytes = 8; \
    for (_i = 0; _i < _numProps; _i++) \
    { \
	_bytes += (8 + ARRAY8_BYTES (strlen (_props[_i]->name)) + \
	    ARRAY8_BYTES (strlen (_props[_i]->type))); \
\
	for (_j = 0; _j < _props[_i]->num_vals; _j++) \
	    _bytes += ARRAY8_BYTES (_props[_i]->vals[_j].length); \
    } \
}


/*
 * STORE FOO
 */

#define STORE_ARRAY8(_pBuf, _len, _array8) \
{ \
    STORE_CARD32 (_pBuf, (CARD32) _len); \
    if (_len) \
        memcpy (_pBuf, _array8, _len); \
    _pBuf += _len + PAD64 (4 + _len); \
}

#define STORE_LISTOF_PROPERTY(_pBuf, _count, _props) \
{ \
    int _i, _j; \
    STORE_CARD32 (_pBuf, _count); \
    _pBuf += 4; \
    for (_i = 0; _i < _count; _i++) \
    { \
        STORE_ARRAY8 (_pBuf, strlen (_props[_i]->name), _props[_i]->name); \
        STORE_ARRAY8 (_pBuf, strlen (_props[_i]->type), _props[_i]->type); \
        STORE_CARD32 (_pBuf, _props[_i]->num_vals); \
        _pBuf += 4; \
        for (_j = 0; _j < _props[_i]->num_vals; _j++) \
	{ \
            STORE_ARRAY8 (_pBuf, _props[_i]->vals[_j].length, \
		(char *) _props[_i]->vals[_j].value); \
	} \
    } \
}

/*
 * Send an ARRAY8 that doesn't fit in the iceConn send buffer.
 */
#define SEND_ARRAY8(_iceConn, _len, _array8) \
{ \
    char _padding[7] = { 0 }; \
    CARD32 _array_len = (CARD32) _len;  \
    IceWriteData32 (_iceConn, 4, &_array_len); \
    if (_len) \
        IceSendData (_iceConn, _len, (char *) _array8); \
    IceSendData (_iceConn, PAD64 (4 + _len), _padding); \
}


/*
 * Client replies not processed by callbacks (we block for them).
 */

typedef struct {
    Status  	status;		/* if 1, client successfully registered */
    char	*client_id;
} _SmcRegisterClientReply;


/*
 * Waiting for Interact
 */

typedef struct _SmcInteractWait {
    SmcInteractProc		interact_proc;
    SmPointer			client_data;
    struct _SmcInteractWait 	*next;
} _SmcInteractWait;


/*
 * Waiting for SaveYourselfPhase2
 */

typedef struct _SmcPhase2Wait {
    SmcSaveYourselfPhase2Proc	phase2_proc;
    SmPointer			client_data;
} _SmcPhase2Wait;


/*
 * Waiting for Properties Reply
 */

typedef struct _SmcPropReplyWait {
    SmcPropReplyProc		prop_reply_proc;
    SmPointer			client_data;
    struct _SmcPropReplyWait 	*next;
} _SmcPropReplyWait;



/*
 * Client connection object
 */

struct _SmcConn {

    /*
     * Some state.
     */

    unsigned int save_yourself_in_progress : 1;
    unsigned int shutdown_in_progress : 1;
    unsigned int unused1 : 6;		     /* future use */
    unsigned int unused2 : 8;		     /* future use */


    /*
     * We use ICE to esablish a connection with the SM.
     */

    IceConn		iceConn;


    /*
     * Major and minor versions of the XSMP.
     */

    int			proto_major_version;
    int			proto_minor_version;


    /*
     * The session manager vendor and release number.
     */

    char		*vendor;
    char		*release;


    /*
     * The Client Id uniquely identifies this client to the session manager.
     */

    char		*client_id;


    /*
     * Callbacks to be invoked when messages arrive from the session manager.
     * These callbacks are specified at SmcOpenConnection time.
     */

    SmcCallbacks	callbacks;


    /*
     * We keep track of all Interact Requests sent by the client.  When the
     * Interact message arrives, we remove it from the list (a FIFO list
     * is maintained).
     */

    _SmcInteractWait	*interact_waits;


    /*
     * If we send a SaveYourselfPhase2Request, we wait for SaveYourselfPhase2.
     */

    _SmcPhase2Wait	*phase2_wait;


    /*
     * We keep track of all Get Properties sent by the client.  When the
     * Properties Reply arrives, we remove it from the list (a FIFO list
     * is maintained).
     */

    _SmcPropReplyWait	*prop_reply_waits;
};



/*
 * Session manager connection object
 */

struct _SmsConn {

    /*
     * Some state.
     */

    unsigned int save_yourself_in_progress : 1;
    unsigned int can_cancel_shutdown : 1;
    unsigned int interact_in_progress : 1;
    unsigned int unused1 : 5;		     /* future use */
    unsigned int unused2 : 8;		     /* future use */


    /*
     * We use ICE to esablish a connection with the client.
     */

    IceConn		iceConn;


    /*
     * Major and minor versions of the XSMP.
     */

    int			proto_major_version;
    int			proto_minor_version;


    /*
     * The Client Id uniquely identifies this client to the session manager.
     */

    char		*client_id;


    /*
     * Callbacks to be invoked when messages arrive from the client.
     */

    SmsCallbacks	callbacks;


    /*
     * What type of interaction is allowed - SmInteractStyle{None,Errors,Any}
     */

    char		interaction_allowed;
};



/*
 * Extern declarations
 */
extern void
_SmcProcessMessage(IceConn iceConn, IcePointer clientData, int opcode,
		   unsigned long length, Bool swap,
		   IceReplyWaitInfo *replyWait, Bool *replyReadyRet);

extern void
_SmsProcessMessage(IceConn iceConn, IcePointer clientData, int opcode,
		   unsigned long length, Bool swap);

extern void
_SmcDefaultErrorHandler(SmcConn smcConn, Bool swap, int offendingMinorOpcode,
			unsigned long offendingSequence, int errorClass,
			int severity, SmPointer values);

extern void
_SmsDefaultErrorHandler(SmsConn smsConn, Bool swap, int offendingMinorOpcode,
			unsigned long offendingSequence, int errorClass,
			int severity, SmPointer values);

extern int     _SmcOpcode;
extern int     _SmsOpcode;

extern SmsNewClientProc	_SmsNewClientProc;
extern SmPointer	_SmsNewClientData;

extern SmcErrorHandler _SmcErrorHandler;
extern SmsErrorHandler _SmsErrorHandler;

#endif /* _SMLIBINT_H_ */
