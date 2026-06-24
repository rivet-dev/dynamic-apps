/*
XRecord.c - client-side library for RECORD extension

Copyright 1995, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/***************************************************************************
 * Copyright 1995 Network Computing Devices
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Network Computing Devices
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 *
 * NETWORK COMPUTING DEVICES DISCLAIMs ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **************************************************************************/
/*
 * By Stephen Gildea, X Consortium, and Martha Zimet, NCD.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <assert.h>
#include <X11/Xlibint.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>
#include <X11/extensions/recordproto.h>
#include <X11/extensions/record.h>
#include <limits.h>

static XExtensionInfo _xrecord_info_data;
static XExtensionInfo *xrecord_info = &_xrecord_info_data;
static const char *xrecord_extension_name = RECORD_NAME;

#define XRecordCheckExtension(dpy,i,val) \
    XextCheckExtension(dpy, i, xrecord_extension_name, val)

/**************************************************************************
 *                                                                        *
 *			   private utility routines                       *
 *                                                                        *
 **************************************************************************/

static XExtDisplayInfo *find_display(Display *dpy);

/*
 * A reply buffer holds a reply from RecordEnableContext.
 * Pieces of that buffer are passed to the XRecordEnableContext callback.
 * ref_count is incremented each time we do that.
 * ref_count is decremented each time XRecordFreeData is called on
 * the buffer.  When ref_count is 0, we can free or reuse the buffer.
 */
struct reply_buffer
{
    struct reply_buffer *next;	/* next in list or NULL */
    unsigned char *buf;		/* pointer to malloc'd buffer */
    int nbytes;			/* size of buf */
    int ref_count;		/* callback uses pending */
};


/*
 * There's some extra information the implementation finds useful
 * to attach to an XRecordInterceptData packet to handle memory
 * management.  So we really allocate one of these.
 */
struct intercept_queue
{
    /* this struct gets passed to the user as an XRecordInterceptData,
       so the data field must come first so we can cast the address
       back and forth */
    XRecordInterceptData data;
    struct intercept_queue *next; /* next in free list or NULL */
    struct mem_cache_str *cache; /* contains head of free list */
};

/*
 * per-display pointers to cache of malloc'd but unused memory
 */
struct mem_cache_str
{
    struct intercept_queue *inter_data;	/* free structs only */
    struct reply_buffer *reply_buffers;	/* all reply buffers */
    int inter_data_count;	/* total allocated, free and in use */
    Bool display_closed;	/* so we know when to free ourself */
};

static int close_display(
    Display *dpy,
    XExtCodes *codes)		/* not used */
{
    XExtDisplayInfo *info = find_display (dpy);

    LockDisplay(dpy);
    if (info && info->data) {
	struct mem_cache_str *cache = (struct mem_cache_str *)info->data;
	struct intercept_queue *iq, *iq_next;
	struct reply_buffer *rbp, **rbp_next_p;

	for (iq=cache->inter_data; iq; iq=iq_next) {
	    iq_next = iq->next;
	    XFree(iq);
	    cache->inter_data_count--;
	}

	/* this is a little trickier, because some of these
	   might still be in use */
	for (rbp_next_p = &cache->reply_buffers; *rbp_next_p; ) {
	    rbp = *rbp_next_p;
	    if (rbp->ref_count == 0) {
		*rbp_next_p = rbp->next;
		XFree(rbp->buf);
		XFree(rbp);
	    } else {
		rbp_next_p = &rbp->next;
	    }
	}

	if (cache->reply_buffers == NULL  &&  cache->inter_data_count == 0) {
	    /* every thing has been freed, can free ourselves, too */
	    XFree(cache);
	} else {
	    cache->display_closed = True;
	    cache->inter_data = NULL; /* neatness only; won't be used */
	}
    }
    UnlockDisplay(dpy);
    return XextRemoveDisplay(xrecord_info, dpy);
}

static XPointer alloc_mem_cache(void)
{
    struct mem_cache_str *cache;

    /* note that an error will go unnoticed */
    cache = Xmalloc(sizeof(struct mem_cache_str));
    if (cache) {
	cache->display_closed = False;
	cache->inter_data = NULL;
	cache->inter_data_count = 0;
	cache->reply_buffers = NULL;
    }
    return (XPointer) cache;
}

static const char *xrecord_error_list[] = {
    "XRecordBadContext (Not a defined RECORD context)",
};

static XEXT_GENERATE_ERROR_STRING (error_string, xrecord_extension_name,
                                   RecordNumErrors, xrecord_error_list)

static XExtensionHooks xrecord_extension_hooks = {
    NULL,                               /* create_gc */
    NULL,                               /* copy_gc */
    NULL,                               /* flush_gc */
    NULL,                               /* free_gc */
    NULL,                               /* create_font */
    NULL,                               /* free_font */
    close_display,                      /* close_display */
    NULL,                      		/* wire_to_event */
    NULL,                      		/* event_to_wire */
    NULL,                               /* error */
    error_string                        /* error_string */
};

static XEXT_GENERATE_FIND_DISPLAY (find_display, xrecord_info,
	xrecord_extension_name, &xrecord_extension_hooks, RecordNumEvents,
	alloc_mem_cache())

/**************************************************************************
 *                                                                        *
 *			   private library routines                       *
 *                                                                        *
 **************************************************************************/

static void
SendRange(
    Display 	*dpy,
    XRecordRange **range_item,
    int   	nranges)
{
    int 		rlen = SIZEOF(xRecordRange);
    while(nranges--)
    {
       xRecordRange xrange;

       xrange.coreRequestsFirst = (*range_item)->core_requests.first;
       xrange.coreRequestsLast = (*range_item)->core_requests.last;
       xrange.coreRepliesFirst = (*range_item)->core_replies.first;
       xrange.coreRepliesLast = (*range_item)->core_replies.last;
       xrange.extRequestsMajorFirst = (*range_item)->ext_requests.ext_major.first;
       xrange.extRequestsMajorLast = (*range_item)->ext_requests.ext_major.last;
       xrange.extRequestsMinorFirst = (*range_item)->ext_requests.ext_minor.first;
       xrange.extRequestsMinorLast = (*range_item)->ext_requests.ext_minor.last;
       xrange.extRepliesMajorFirst = (*range_item)->ext_replies.ext_major.first;
       xrange.extRepliesMajorLast = (*range_item)->ext_replies.ext_major.last;
       xrange.extRepliesMinorFirst = (*range_item)->ext_replies.ext_minor.first;
       xrange.extRepliesMinorLast = (*range_item)->ext_replies.ext_minor.last;
       xrange.deliveredEventsFirst = (*range_item)->delivered_events.first;
       xrange.deliveredEventsLast = (*range_item)->delivered_events.last;
       xrange.deviceEventsFirst = (*range_item)->device_events.first;
       xrange.deviceEventsLast = (*range_item)->device_events.last;
       xrange.errorsFirst = (*range_item)->errors.first;
       xrange.errorsLast = (*range_item)->errors.last;
       xrange.clientStarted = (*range_item)->client_started;
       xrange.clientDied = (*range_item)->client_died;

       Data(dpy, (char *)&xrange, rlen);
       range_item++;
    }
}

/**************************************************************************
 *                                                                        *
 *		    public routines               			  *
 *                                                                        *
 **************************************************************************/

XID
XRecordIdBaseMask(Display *dpy)
{
    return 0x1fffffff & ~dpy->resource_mask;
}

Status
XRecordQueryVersion(Display *dpy, int *cmajor_return, int *cminor_return)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xRecordQueryVersionReq   	*req;
    xRecordQueryVersionReply 		rep;

    XRecordCheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(RecordQueryVersion, req);
    req->reqType = info->codes->major_opcode;
    req->recordReqType = X_RecordQueryVersion;
    req->majorVersion = RECORD_MAJOR_VERSION;
    req->minorVersion = RECORD_MINOR_VERSION;
    if (!_XReply(dpy,(xReply *)&rep, 0, True)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }
    UnlockDisplay(dpy);
    SyncHandle();
    *cmajor_return = rep.majorVersion;
    *cminor_return = rep.minorVersion;
    return ((rep.majorVersion == RECORD_MAJOR_VERSION) &&
	    (rep.minorVersion >= RECORD_LOWEST_MINOR_VERSION));
}

XRecordContext
XRecordCreateContext(Display *dpy, int datum_flags,
		     XRecordClientSpec *clients, int nclients,
		     XRecordRange **ranges, int nranges)
{
    XExtDisplayInfo 	*info = find_display (dpy);
    register xRecordCreateContextReq 	*req;
    int			clen = 4 * nclients;

    XRecordCheckExtension (dpy, info, 0);
    LockDisplay(dpy);
    GetReq(RecordCreateContext, req);

    req->reqType = info->codes->major_opcode;
    req->recordReqType = X_RecordCreateContext;
    req->context = XAllocID(dpy);
    req->length += (nclients * 4 +
		    nranges * SIZEOF(xRecordRange)) >> 2;
    req->elementHeader = datum_flags;
    req->nClients = nclients;
    req->nRanges = nranges;

    Data32(dpy, (long *)clients, clen);
    SendRange(dpy, ranges, nranges);

    UnlockDisplay(dpy);
    SyncHandle();
    return req->context;
}

XRecordRange *
XRecordAllocRange(void)
{
    return Xcalloc(1, sizeof(XRecordRange));
}

Status
XRecordRegisterClients(Display *dpy, XRecordContext context, int datum_flags,
		       XRecordClientSpec *clients, int nclients,
		       XRecordRange **ranges, int nranges)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xRecordRegisterClientsReq 	*req;
    int			clen = 4 * nclients;

    XRecordCheckExtension (dpy, info, 0);
    LockDisplay(dpy);
    GetReq(RecordRegisterClients, req);

    req->reqType = info->codes->major_opcode;
    req->recordReqType = X_RecordRegisterClients;
    req->context =  context;
    req->length += (nclients * 4 +
		    nranges * SIZEOF(xRecordRange)) >> 2;
    req->elementHeader = datum_flags;
    req->nClients = nclients;
    req->nRanges = nranges;

    Data32(dpy, (long *)clients, clen);
    SendRange(dpy, ranges, nranges);

    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

Status
XRecordUnregisterClients(Display *dpy, XRecordContext context,
			 XRecordClientSpec *clients, int nclients)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xRecordUnregisterClientsReq 	*req;
    int			clen = 4 * nclients;

    XRecordCheckExtension (dpy, info, 0);
    LockDisplay(dpy);
    GetReq(RecordUnregisterClients, req);

    req->reqType = info->codes->major_opcode;
    req->recordReqType = X_RecordUnregisterClients;
    req->context = context;
    req->length += nclients;
    req->nClients = nclients;

    Data32(dpy, (long *)clients, clen);

    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

static void
WireToLibRange(
    xRecordRange *wire_range,
    XRecordRange *lib_range)
{
    lib_range->core_requests.first = wire_range->coreRequestsFirst;
    lib_range->core_requests.last = wire_range->coreRequestsLast;
    lib_range->core_replies.first = wire_range->coreRepliesFirst;
    lib_range->core_replies.last = wire_range->coreRepliesLast;
    lib_range->ext_requests.ext_major.first = wire_range->extRequestsMajorFirst;
    lib_range->ext_requests.ext_major.last = wire_range->extRequestsMajorLast;
    lib_range->ext_requests.ext_minor.first = wire_range->extRequestsMinorFirst;
    lib_range->ext_requests.ext_minor.last = wire_range->extRequestsMinorLast;
    lib_range->ext_replies.ext_major.first = wire_range->extRepliesMajorFirst;
    lib_range->ext_replies.ext_major.last = wire_range->extRepliesMajorLast;
    lib_range->ext_replies.ext_minor.first = wire_range->extRepliesMinorFirst;
    lib_range->ext_replies.ext_minor.last = wire_range->extRepliesMinorLast;
    lib_range->delivered_events.first = wire_range->deliveredEventsFirst;
    lib_range->delivered_events.last = wire_range->deliveredEventsLast;
    lib_range->device_events.first = wire_range->deviceEventsFirst;
    lib_range->device_events.last = wire_range->deviceEventsLast;
    lib_range->errors.first = wire_range->errorsFirst;
    lib_range->errors.last = wire_range->errorsLast;
    lib_range->client_started = wire_range->clientStarted;
    lib_range->client_died = wire_range->clientDied;
}

Status
XRecordGetContext(Display *dpy, XRecordContext context,
		  XRecordState **state_return)
{
    XExtDisplayInfo 	*info = find_display (dpy);
    register 		xRecordGetContextReq   	*req;
    xRecordGetContextReply 	rep;
    unsigned int	count;
    xRecordRange   	xrange;
    xRecordClientInfo   xclient_inf;
    XRecordState	*ret;

    XRecordCheckExtension (dpy, info, 0);
    LockDisplay(dpy);
    GetReq(RecordGetContext, req);
    req->reqType = info->codes->major_opcode;
    req->recordReqType = X_RecordGetContext;
    req->context = context;
    if (!_XReply(dpy,(xReply *)&rep, 0, False)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return 0;
    }
    count = rep.nClients;

    ret = Xmalloc(sizeof(XRecordState));
    if (!ret) {
	_XEatDataWords (dpy, rep.length);
	UnlockDisplay(dpy);
	SyncHandle();
	return 0;
    }

    ret->enabled = rep.enabled;
    ret->datum_flags = rep.elementHeader;
    ret->nclients = count;

    if (count)
    {
	XRecordClientInfo	**client_inf = NULL;
	XRecordClientInfo	*client_inf_str = NULL;

	if (count < (INT_MAX / sizeof(XRecordClientInfo))) {
	    client_inf = Xcalloc(count, sizeof(XRecordClientInfo *));
	    if (client_inf != NULL)
		client_inf_str = Xmalloc(count * sizeof(XRecordClientInfo));
	}
	ret->client_info = client_inf;
        if (!client_inf || !client_inf_str)
        {
	   _XEatDataWords (dpy, rep.length);
	   UnlockDisplay(dpy);
	   XRecordFreeState(ret); /* frees ret->client_info, aka client_inf */
	   SyncHandle();
	   return 0;
        }
        for (unsigned int i = 0; i < count; i++)
        {
	    client_inf[i] = &(client_inf_str[i]);
            _XRead(dpy, (char *)&xclient_inf, (long)sizeof(xRecordClientInfo));
            client_inf_str[i].client = xclient_inf.clientResource;
            client_inf_str[i].nranges = xclient_inf.nRanges;

	    if (xclient_inf.nRanges)
	    {
		XRecordRange	*ranges = NULL;

		if (xclient_inf.nRanges < (INT_MAX / sizeof(XRecordRange))) {
		    client_inf_str[i].ranges =
			Xcalloc(xclient_inf.nRanges, sizeof(XRecordRange *));
		    if (client_inf_str[i].ranges != NULL)
			ranges =
			    Xmalloc(xclient_inf.nRanges * sizeof(XRecordRange));
		}
		else
		    client_inf_str[i].ranges = NULL;

		if (!client_inf_str[i].ranges || !ranges) {
		    /* XXX eat data */
		    UnlockDisplay(dpy);
		    XRecordFreeState(ret);
		    SyncHandle();
		    return 0;
		}
		for (unsigned int rn = 0; rn < xclient_inf.nRanges; rn++) {
		    client_inf_str[i].ranges[rn] = &(ranges[rn]);
		    _XRead(dpy, (char *)&xrange, (long)sizeof(xRecordRange));
		    WireToLibRange(&xrange, &(ranges[rn]));
		}
	    } else {
		client_inf_str[i].ranges = NULL;
	    }
        }
    } else {
	ret->client_info = NULL;
    }

    *state_return = ret;

    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

void
XRecordFreeState(XRecordState *state)
{
    if (state->client_info) {
	for (unsigned long i = 0; i < state->nclients; i++) {
	    if (state->client_info[i]->ranges) {
		if (state->client_info[i]->ranges[0])
		    Xfree(state->client_info[i]->ranges[0]);
		Xfree(state->client_info[i]->ranges);
	    }
	}
	if (state->client_info[0])
	    Xfree(state->client_info[0]);
	Xfree(state->client_info);
    }
    Xfree(state);
}

static struct reply_buffer *alloc_reply_buffer(
    XExtDisplayInfo *info,
    int nbytes)
{
    struct mem_cache_str *cache = (struct mem_cache_str *)info->data;
    struct reply_buffer *rbp;
    struct reply_buffer *saved_rb = NULL;
    /*
     * First look for an allocated buffer that is not in use.
     * If we have a big enough buffer, use that, otherwise
     * realloc an existing one.
     */
    for (rbp = cache->reply_buffers; rbp; rbp = rbp->next) {
	if (rbp->ref_count == 0) {
	    if (rbp->nbytes >= nbytes)
		return rbp;
	    else
		saved_rb = rbp;
	}
    }
    if (saved_rb) {
	saved_rb->buf = (unsigned char *)Xrealloc(saved_rb->buf, nbytes);
	if (!saved_rb->buf) {
	    saved_rb->nbytes = 0;
	    return NULL;
	}
	saved_rb->nbytes = nbytes;
	return saved_rb;
    }

    /*
     * nothing available; malloc a new struct
     */
    rbp = Xmalloc(sizeof(struct reply_buffer));
    if (!rbp)
	return NULL;
    rbp->buf = Xmalloc(nbytes);
    if (!rbp->buf) {
	Xfree(rbp);
	return NULL;
    }
    rbp->nbytes = nbytes;
    rbp->ref_count = 0;
    rbp->next = cache->reply_buffers;
    cache->reply_buffers = rbp;
    return rbp;
}

static XRecordInterceptData *alloc_inter_data(XExtDisplayInfo *info)
{
    struct mem_cache_str *cache = (struct mem_cache_str *)info->data;
    struct intercept_queue *iq;

    /* if there is one on the free list, pop it */
    if (cache->inter_data) {
	iq = cache->inter_data;
	cache->inter_data = iq->next;
	return &iq->data;
    }
    /* allocate a new one */
    iq = Xmalloc(sizeof(struct intercept_queue));
    if (!iq)
	return NULL;
    iq->cache = cache;
    cache->inter_data_count++;
    return &iq->data;
}

void
XRecordFreeData(XRecordInterceptData *data)
{
    /* we can do this cast because that is what we really allocated */
    struct intercept_queue *iq = (struct intercept_queue *)data;
    struct reply_buffer *rbp = NULL;
    struct mem_cache_str *cache = iq->cache;

    /*
     * figure out what reply_buffer this points at
     * and decrement its ref_count.
     */
    if (data->data) {

	for (rbp = cache->reply_buffers; rbp; rbp = rbp->next) {
	    if (data->data >= rbp->buf
		&& data->data < rbp->buf + rbp->nbytes)
	    {
		assert(rbp->ref_count > 0);
		rbp->ref_count--;
		break;
	    }
	}
	/* it's an error if we didn't find something to free */
	assert(rbp);
    }
    /*
     * If the display is still open, put this back on the free queue.
     *
     * Otherwise the display is closed and we won't reuse this, so free it.
     * See if we can free the reply buffer, too.
     * If we can, see if this is the last reply buffer and if so
     * free the list of reply buffers.
     */
    if (cache->display_closed == False) {
	iq->next = cache->inter_data;
	cache->inter_data = iq;
    } else {
	if (rbp && rbp->ref_count == 0) {
	    struct reply_buffer *rbp2, **rbp_next_p;

	    /* Have to search the list again to find the prev element.
	       This is not the common case, so don't slow down the code
	       above by doing it then. */
	    for (rbp_next_p = &cache->reply_buffers; *rbp_next_p; ) {
		rbp2 = *rbp_next_p;
		if (rbp == rbp2) {
		    *rbp_next_p = rbp2->next;
		    break;
		} else {
		    rbp_next_p = &rbp2->next;
		}
	    }
	    XFree(rbp->buf);
	    XFree(rbp);
	}

	XFree(iq);
	cache->inter_data_count--;

	if (cache->reply_buffers == NULL  &&  cache->inter_data_count == 0) {
	    XFree(cache); /* all finished */
	}
    }
}

/* the EXTRACT macros are adapted from ICElibint.h */

#ifndef WORD64

#define EXTRACT_CARD16(swap, src, dst) \
{ \
    (dst) = *((CARD16 *) (src)); \
    if (swap) \
        (dst) = lswaps (dst); \
}

#define EXTRACT_CARD32(swap, src, dst) \
{ \
    (dst) = *((CARD32 *) (src)); \
    if (swap) \
        (dst) = lswapl (dst); \
}

#else /* WORD64 */

#define EXTRACT_CARD16(swap, src, dst) \
{ \
    (dst) = *((src) + 0); \
    (dst) <<= 8; \
    (dst) |= *((src) + 1); \
    if (swap) \
        (dst) = lswaps (dst); \
}

#define EXTRACT_CARD32(swap, src, dst) \
{ \
    (dst) = *((src) + 0); \
    (dst) <<= 8; \
    (dst) |= *((src) + 1); \
    (dst) <<= 8; \
    (dst) |= *((src) + 2); \
    (dst) <<= 8; \
    (dst) |= *((src) + 3); \
    if (swap) \
        (dst) = lswapl (dst); \
}

#endif /* WORD64 */

/* byte swapping macros from xfs/include/misc.h */

/* byte swap a long literal */
#define lswapl(x) ((((x) & 0xff) << 24) |\
		   (((x) & 0xff00) << 8) |\
		   (((x) & 0xff0000) >> 8) |\
		   (((x) >> 24) & 0xff))

/* byte swap a short literal */
#define lswaps(x) ((((x) & 0xff) << 8) | (((x) >> 8) & 0xff))

enum parser_return { Continue, End, Error };

static enum parser_return
parse_reply_call_callback(
    Display *dpy,
    XExtDisplayInfo *info,
    xRecordEnableContextReply *rep,
    struct reply_buffer *reply,
    XRecordInterceptProc callback,
    XPointer		 closure)
{
    XRecordInterceptData *data;
    unsigned int current_index;
    int datum_bytes = 0;

    /* call the callback for each protocol element in the reply */
    current_index = 0;
    do {
	data = alloc_inter_data(info);
	if (!data)
	    return Error;

	data->id_base = rep->idBase;
	data->category = rep->category;
	data->client_swapped = rep->clientSwapped;
	data->server_time = rep->serverTime;
	data->client_seq = rep->recordedSequenceNumber;
	/*
	 * compute the size of this protocol element.
	 */
	switch (rep->category) {
	case XRecordFromServer:
	    if (reply == NULL)
		goto out;
	    if (rep->elementHeader&XRecordFromServerTime) {
		if (current_index + 4 > rep->length << 2)
		    return Error;
		EXTRACT_CARD32(rep->clientSwapped,
			       reply->buf+current_index,
			       data->server_time);
		current_index += 4;
	    }
	    if (current_index + 1 > rep->length << 2)
		goto out;
	    switch (reply->buf[current_index]) {
	    case X_Reply: /* reply */
		if (current_index + 8 > rep->length << 2)
		    goto out;
		EXTRACT_CARD32(rep->clientSwapped,
			       reply->buf+current_index+4, datum_bytes);
		if (datum_bytes < 0 || datum_bytes > ((INT_MAX >> 2) - 8))
		    goto out;
		datum_bytes = (datum_bytes+8) << 2;
		break;
	    default: /* error or event */
		datum_bytes = 32;
	    }
	    break;
	case XRecordFromClient:
	    if (reply == NULL)
		goto out;
	    if (rep->elementHeader&XRecordFromClientTime) {
		if (current_index + 4 > rep->length << 2)
		    goto out;
		EXTRACT_CARD32(rep->clientSwapped,
			       reply->buf+current_index,
			       data->server_time);
		current_index += 4;
	    }
	    if (rep->elementHeader&XRecordFromClientSequence) {
		if (current_index + 4 > rep->length << 2)
		    goto out;
		EXTRACT_CARD32(rep->clientSwapped,
			       reply->buf+current_index,
			       data->client_seq);
		current_index += 4;
	    }
	    if (current_index + 4 > rep->length<<2)
		goto out;
	    if (reply->buf[current_index+2] == 0
		&& reply->buf[current_index+3] == 0) /* needn't swap 0 */
	    {	/* BIG-REQUESTS */
		if (current_index + 8 > rep->length << 2)
		    goto out;
		EXTRACT_CARD32(rep->clientSwapped,
			       reply->buf+current_index+4, datum_bytes);
	    } else {
		EXTRACT_CARD16(rep->clientSwapped,
			       reply->buf+current_index+2, datum_bytes);
	    }
	    if (datum_bytes < 0 || datum_bytes > INT_MAX >> 2)
		goto out;
	    datum_bytes <<= 2;
	    break;
	case XRecordClientStarted:
	    if (reply == NULL)
		goto out;
	    if (current_index + 8 > rep->length << 2)
		goto out;
	    EXTRACT_CARD16(rep->clientSwapped,
			   reply->buf+current_index+6, datum_bytes);
	    datum_bytes = (datum_bytes+2) << 2;
	    break;
	case XRecordClientDied:
	    if (rep->elementHeader&XRecordFromClientSequence) {
		if (reply == NULL)
		    goto out;
		if (current_index + 4 > rep->length << 2)
		    goto out;
		EXTRACT_CARD32(rep->clientSwapped,
			       reply->buf+current_index,
			       data->client_seq);
		current_index += 4;
	    } else if (current_index < rep->length << 2)
		goto out;
	    datum_bytes = 0;
	    break;
	case XRecordStartOfData:
	case XRecordEndOfData:
	    if (current_index < rep->length << 2)
		goto out;
	    datum_bytes = 0;
	    break;
	}

	if (datum_bytes > 0) {
	    if (INT_MAX - datum_bytes < (rep->length << 2) - current_index) {
		fprintf(stderr,
			"XRecord: %lu-byte reply claims %d-byte element (seq %lu)\n",
			(unsigned long)rep->length << 2, current_index + datum_bytes,
			dpy->last_request_read);
		goto out;
	    }
	    /*
	     * This assignment (and indeed the whole buffer sharing
	     * scheme) assumes arbitrary 4-byte boundaries are
	     * addressable.
	     */
	    data->data = reply->buf+current_index;
	    reply->ref_count++;
	} else {
	    data->data = NULL;
	}
	data->data_len = datum_bytes >> 2;

	(*callback)(closure, data);

	current_index += datum_bytes;
    } while (current_index<rep->length<<2);

    if (rep->category == XRecordEndOfData)
	return End;

    return Continue;
out:
    Xfree(data);
    return Error;
}

Status
XRecordEnableContext(Display *dpy, XRecordContext context,
		     XRecordInterceptProc callback, XPointer closure)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xRecordEnableContextReq   	*req;
    xRecordEnableContextReply 	rep;
    struct reply_buffer *reply;

    XRecordCheckExtension (dpy, info, 0);
    LockDisplay(dpy);
    GetReq(RecordEnableContext, req);

    req->reqType = info->codes->major_opcode;
    req->recordReqType = X_RecordEnableContext;
    req->context = context;

    while (1)
    {
	enum parser_return status;

	/* This code should match that in XRecordEnableContextAsync */
	if (!_XReply (dpy, (xReply *)&rep, 0, xFalse))
	{
	    UnlockDisplay(dpy);
	    SyncHandle();
	    return 0;
	}

	if (rep.length > INT_MAX >> 2) {
	    UnlockDisplay(dpy);
	    SyncHandle();
	    return 0;
	}

	if (rep.length > 0) {
	    reply = alloc_reply_buffer(info, rep.length<<2);
	    if (!reply) {
		UnlockDisplay(dpy);
		SyncHandle();
		return 0;
	    }
	    _XRead (dpy, (char *)reply->buf, rep.length<<2);
	} else {
	    reply = NULL;
	}

	status = parse_reply_call_callback(dpy, info, &rep, reply,
					   callback, closure);
	switch (status) {
	case Continue:
	    break;
	case End:
	    UnlockDisplay(dpy);
	    SyncHandle();
	    return 1;
	case Error:
	    UnlockDisplay(dpy);
	    SyncHandle();
	    return 0;
	}
    }
}


typedef struct _record_async_state
{
    unsigned long enable_seq;
    _XAsyncHandler *async;
    _XAsyncErrorState *error_state;
    XExtDisplayInfo *info;
    XRecordInterceptProc callback;
    XPointer closure;
} record_async_state;

static Bool
record_async_handler(
    register Display *dpy,
    register xReply *rep,
    char *buf,
    int len,
    XPointer adata)
{
    register record_async_state *state = (record_async_state *)adata;
    struct reply_buffer *reply;
    enum parser_return status;

    if (dpy->last_request_read != state->enable_seq)
    {
	if (dpy->last_request_read > state->enable_seq) {
	    /* it is an error that we are still on the handler list */
	    fprintf(stderr, "XRecord: handler for seq %lu never saw XRecordEndOfData.  (seq now %lu)\n",
		    state->enable_seq, dpy->last_request_read);
	    DeqAsyncHandler(dpy, state->async);
	    Xfree(state->async);
	}
	return False;
    }
    if (rep->generic.type == X_Error)
    {
	DeqAsyncHandler(dpy, state->async);
	Xfree(state->async);
	return False;
    }

    if (rep->generic.length > 0) {
	reply = alloc_reply_buffer(state->info, rep->generic.length<<2);

	if (!reply) {
	    DeqAsyncHandler(dpy, state->async);
	    Xfree(state->async);
	    return False;
	}

	_XGetAsyncData(dpy, (char *)reply->buf, buf, len,
		       SIZEOF(xRecordEnableContextReply),
		       rep->generic.length << 2, 0);
    } else {
	reply = NULL;
    }

    status = parse_reply_call_callback(dpy, state->info,
				       (xRecordEnableContextReply*) rep,
				       reply, state->callback, state->closure);

    if (status != Continue)
    {
	DeqAsyncHandler(dpy, state->async);
	Xfree(state->async);
	if (status == Error)
	    return False;
    }

    return True;
}

/*
 * reads the first reply, StartOfData, synchronously,
 * then returns allowing the app to call XRecordProcessReplies
 * to get the rest.
 */
Status
XRecordEnableContextAsync(Display *dpy, XRecordContext context,
			  XRecordInterceptProc callback, XPointer closure)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xRecordEnableContextReq *req;
    xRecordEnableContextReply rep;
    struct reply_buffer *reply;
    enum parser_return status;
    _XAsyncHandler *async;
    record_async_state *async_state;

    XRecordCheckExtension (dpy, info, 0);
    async = Xmalloc(sizeof(_XAsyncHandler) + sizeof(record_async_state));
    if (!async)
	return 0;
    async_state = (record_async_state *)(async + 1);

    LockDisplay(dpy);
    GetReq(RecordEnableContext, req);

    req->reqType = info->codes->major_opcode;
    req->recordReqType = X_RecordEnableContext;
    req->context = context;

    /* Get the StartOfData reply. */
    /* This code should match that in XRecordEnableContext */
    if (!_XReply (dpy, (xReply *)&rep, 0, xFalse))
    {
	UnlockDisplay(dpy);
	SyncHandle();
	Xfree(async);
	return 0;
    }

    /* this had better be a StartOfData, which has no extra data. */
    if (rep.length != 0) {
	fprintf(stderr, "XRecord: malformed StartOfData for sequence %lu\n",
		dpy->last_request_read);
    }
    reply = NULL;

    status = parse_reply_call_callback(dpy, info, &rep, reply,
				       callback, closure);
    if (status != Continue)
    {
	UnlockDisplay(dpy);
	Xfree(async);
	return 0;
    }

    /* hook in the async handler for the rest of the replies */
    async_state->enable_seq = dpy->request;
    async_state->async = async;
    async_state->info = info;
    async_state->callback = callback;
    async_state->closure = closure;

    async->next = dpy->async_handlers;
    async->handler = record_async_handler;
    async->data = (XPointer)async_state;
    dpy->async_handlers = async;

    UnlockDisplay(dpy);
    /* Don't invoke SyncHandle here, since this is an async
       function.  Does this break XSetAfterFunction() ? */
    return 1;
}

void
XRecordProcessReplies(Display *dpy)
{
    (void) XPending(dpy);
}

Status
XRecordDisableContext(Display *dpy, XRecordContext context)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xRecordDisableContextReq 	*req;

    XRecordCheckExtension (dpy, info, 0);
    LockDisplay(dpy);
    GetReq(RecordDisableContext, req);
    req->reqType = info->codes->major_opcode;
    req->recordReqType = X_RecordDisableContext;
    req->context =  context;

    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

Status
XRecordFreeContext(Display *dpy, XRecordContext context)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xRecordFreeContextReq 	*req;

    XRecordCheckExtension (dpy, info, 0);

    LockDisplay(dpy);
    GetReq(RecordFreeContext, req);
    req->reqType = info->codes->major_opcode;
    req->recordReqType = X_RecordFreeContext;
    req->context = context;

    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}
