/******************************************************************************


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

Author: Ralph Mor, X Consortium
******************************************************************************/

#ifdef WIN32
#define _WILLWINSOCK_
#endif
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/ICE/ICElib.h>
#include "ICElibint.h"
#include <X11/Xtrans/Xtrans.h>
#include <stdio.h>
#ifdef WIN32
#include <X11/Xwinsock.h>
#include <X11/Xw32defs.h>
#endif


/*
 * scratch buffer
 */

char *
IceAllocScratch (
	IceConn		iceConn,
	unsigned long	size
)
{
    if (!iceConn->scratch || size > iceConn->scratch_size)
    {
	free (iceConn->scratch);

	iceConn->scratch = malloc (size);
	iceConn->scratch_size = size;
    }

    return (iceConn->scratch);
}

/*
 * Fatal IO error.  First notify each protocol's IceIOErrorProc
 * callback, then invoke the application IO error handler.
 */

static void _X_COLD
IceFatalIOError (
	IceConn	iceConn
)
{
    iceConn->io_ok = False;

    if (iceConn->connection_status == IceConnectPending)
    {
	/*
	 * Don't invoke IO error handler if we are in the
	 * middle of a connection setup.
	 */

	return;
    }

    if (iceConn->process_msg_info)
    {
	for (int i = iceConn->his_min_opcode;
	     i <= iceConn->his_max_opcode; i++)
	{
	    _IceProcessMsgInfo *process;

	    process = &iceConn->process_msg_info[i - iceConn->his_min_opcode];

	    if ((process != NULL) && process->in_use)
	    {
		IceIOErrorProc IOErrProc = process->accept_flag ?
		    process->protocol->accept_client->io_error_proc :
		    process->protocol->orig_client->io_error_proc;

		if (IOErrProc)
		    (*IOErrProc) (iceConn);
	    }
	}
    }

    (*_IceIOErrorHandler) (iceConn);
    return;
}

/*
 * Output/Input buffer functions
 */

int
IceFlush (
	IceConn iceConn
)
{
    /*
     * Should be impossible, unless we messed up our buffer math somewhere,
     * or one of our pointers has been corrupted.
     */
    if (_X_UNLIKELY(iceConn->outbufptr > iceConn->outbufmax))
    {
	IceFatalIOError (iceConn);
	return 0;
    }

    _IceWrite (iceConn,
	(unsigned long) (iceConn->outbufptr - iceConn->outbuf),
	iceConn->outbuf);

    iceConn->outbufptr = iceConn->outbuf;
    return 1;
}


int
IceGetOutBufSize (
	IceConn iceConn
)
{
    return (iceConn->outbufmax - iceConn->outbuf);
}


int
IceGetInBufSize (
	IceConn iceConn
)
{
    return (iceConn->inbufmax - iceConn->inbuf);
}



/*
 * informational functions
 */

IceConnectStatus
IceConnectionStatus (
	IceConn iceConn
)
{
    return (iceConn->connection_status);
}


char *
IceVendor (
	IceConn iceConn
)
{
    return strdup(iceConn->vendor);
}


char *
IceRelease (
	IceConn iceConn
)
{
    return strdup(iceConn->release);
}


int
IceProtocolVersion (
	IceConn iceConn
)
{
    return (_IceVersions[iceConn->my_ice_version_index].major_version);
}


int
IceProtocolRevision (
	IceConn iceConn
)
{
    return (_IceVersions[iceConn->my_ice_version_index].minor_version);
}


int
IceConnectionNumber (
	IceConn iceConn
)
{
    return (_IceTransGetConnectionNumber (iceConn->trans_conn));
}


char *
IceConnectionString (
	IceConn iceConn
)
{
    if (iceConn->connection_string)
    {
	return strdup(iceConn->connection_string);
    }
    else
	return (NULL);
}


unsigned long
IceLastSentSequenceNumber (
	IceConn iceConn
)
{
    return (iceConn->send_sequence);
}


unsigned long
IceLastReceivedSequenceNumber (
	IceConn iceConn
)
{
    return (iceConn->receive_sequence);
}


Bool
IceSwapping (
	IceConn iceConn
)
{
    return (iceConn->swap);
}



/*
 * Read "n" bytes from a connection.
 *
 * Return Status 0 if we detected an EXPECTED closed connection.
 *
 */

Status
_IceRead (
	register IceConn iceConn,
	unsigned long	 nbytes,
	register char	 *ptr
)
{
    register unsigned long nleft;

    nleft = nbytes;
    while (nleft > 0)
    {
	int nread;

	if (iceConn->io_ok)
	    nread = _IceTransRead (iceConn->trans_conn, ptr, (int) nleft);
	else
	    return (1);

	if (nread <= 0)
	{
#ifdef WIN32
	    errno = WSAGetLastError();
#endif
	    if (iceConn->want_to_close)
	    {
		/*
		 * We sent a WantToClose message and now we detected that
		 * the other side closed the connection.
		 */

		_IceConnectionClosed (iceConn);	    /* invoke watch procs */

		return (0);
	    }
	    else
	    {
		IceFatalIOError (iceConn);
		return (1);
	    }
	}

	nleft -= nread;
	ptr   += nread;
    }

    return (1);
}



/*
 * If we read a message header with a bad major or minor opcode,
 * we need to advance to the end of the message.  This way, the next
 * message can be processed correctly.
 */

void
_IceReadSkip (
	register IceConn	iceConn,
	register unsigned long	nbytes
)
{
    char temp[512];

    while (nbytes > 0)
    {
	unsigned long rbytes = nbytes > 512 ? 512 : nbytes;

	_IceRead (iceConn, rbytes, temp);
	nbytes -= rbytes;
    }
}



/*
 * Write "n" bytes to a connection.
 */

void
_IceWrite (
	register IceConn iceConn,
	unsigned long	 nbytes,
	register char	 *ptr
)
{
    register unsigned long nleft;

    nleft = nbytes;
    while (nleft > 0)
    {
	int nwritten;

	if (iceConn->io_ok)
	    nwritten = _IceTransWrite (iceConn->trans_conn, ptr, (int) nleft);
	else
	    return;

	if (nwritten <= 0)
	{
#ifdef WIN32
	    errno = WSAGetLastError();
#endif
	    IceFatalIOError (iceConn);
	    return;
	}

	nleft -= nwritten;
	ptr   += nwritten;
    }
}



void
_IceAddOpcodeMapping (
	IceConn	iceConn,
	int 	hisOpcode,
	int 	myOpcode
)
{
    if (hisOpcode <= 0 || hisOpcode > 255)
    {
	return;
    }
    else if (iceConn->process_msg_info == NULL)
    {
	iceConn->process_msg_info = malloc (sizeof (_IceProcessMsgInfo));
	iceConn->his_min_opcode = iceConn->his_max_opcode = hisOpcode;
    }
    else if (hisOpcode < iceConn->his_min_opcode)
    {
	_IceProcessMsgInfo *oldVec = iceConn->process_msg_info;
	int oldsize = iceConn->his_max_opcode - iceConn->his_min_opcode + 1;
	int newsize = iceConn->his_max_opcode - hisOpcode + 1;
	int i;

	iceConn->process_msg_info = malloc (
	    newsize * sizeof (_IceProcessMsgInfo));

	memcpy (&iceConn->process_msg_info[
	    iceConn->his_min_opcode - hisOpcode], oldVec,
	    oldsize * sizeof (_IceProcessMsgInfo));

	free (oldVec);

	for (i = hisOpcode + 1; i < iceConn->his_min_opcode; i++)
	{
	    iceConn->process_msg_info[i -
		iceConn->his_min_opcode].in_use = False;

	    iceConn->process_msg_info[i -
		iceConn->his_min_opcode].protocol = NULL;
	}

	iceConn->his_min_opcode = hisOpcode;
    }
    else if (hisOpcode > iceConn->his_max_opcode)
    {
	_IceProcessMsgInfo *oldVec = iceConn->process_msg_info;
	int oldsize = iceConn->his_max_opcode - iceConn->his_min_opcode + 1;
	int newsize = hisOpcode - iceConn->his_min_opcode + 1;
	int i;

	iceConn->process_msg_info = malloc (
	    newsize * sizeof (_IceProcessMsgInfo));

	memcpy (iceConn->process_msg_info, oldVec,
	    oldsize * sizeof (_IceProcessMsgInfo));

	free (oldVec);

	for (i = iceConn->his_max_opcode + 1; i < hisOpcode; i++)
	{
	    iceConn->process_msg_info[i -
		iceConn->his_min_opcode].in_use = False;

	    iceConn->process_msg_info[i -
		iceConn->his_min_opcode].protocol = NULL;
	}

	iceConn->his_max_opcode = hisOpcode;
    }

    iceConn->process_msg_info[hisOpcode -
	iceConn->his_min_opcode].in_use = True;

    iceConn->process_msg_info[hisOpcode -
	iceConn->his_min_opcode].my_opcode = myOpcode;

    iceConn->process_msg_info[hisOpcode -
	iceConn->his_min_opcode].protocol = &_IceProtocols[myOpcode - 1];
}



char *
IceGetPeerName (IceConn iceConn)
{
    return (_IceTransGetPeerNetworkId (iceConn->trans_conn));
}


char *
_IceGetPeerName (IceConn iceConn)
{
    return (IceGetPeerName(iceConn));
}
