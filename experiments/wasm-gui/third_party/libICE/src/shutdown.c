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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/ICE/ICElib.h>
#include "ICElibint.h"
#include <X11/Xtrans/Xtrans.h>


Status
IceProtocolShutdown (
	IceConn iceConn,
	int	majorOpcode
)
{
    int i;

    if (iceConn->proto_ref_count == 0 || iceConn->process_msg_info == NULL ||
        majorOpcode < 1 || majorOpcode > _IceLastMajorOpcode)
    {
	return (0);
    }


    /*
     * Make sure this majorOpcode is really being used.
     */

    for (i = iceConn->his_min_opcode; i <= iceConn->his_max_opcode; i++)
    {
	int n = i - iceConn->his_min_opcode;
	if (iceConn->process_msg_info[n].in_use &&
	    iceConn->process_msg_info[n].my_opcode == majorOpcode)
	{

	    /*
	     * OK, we can shut down the protocol.
	     */

	    iceConn->process_msg_info[n].in_use = False;
	    iceConn->proto_ref_count--;
	    return (1);
	}
    }

    return (0);
}



void
IceSetShutdownNegotiation (
	IceConn     	iceConn,
	Bool		negotiate
)
{
    iceConn->skip_want_to_close = negotiate ? False : True;
}



Bool
IceCheckShutdownNegotiation (
	IceConn     iceConn
)
{
    return (iceConn->skip_want_to_close ? False : True);
}



IceCloseStatus
IceCloseConnection (
	IceConn     iceConn
)
{
    int refCountReachedZero;
    IceCloseStatus status;

    /*
     * If this connection object was never valid, we can close
     * it right now.  This happens if IceAcceptConnection was
     * called, but after calling IceProcessMessages several times
     * the connection was rejected (because of authentication or
     * some other reason).
     */

    if (iceConn->listen_obj &&
	iceConn->connection_status != IceConnectAccepted)
    {
	_IceConnectionClosed (iceConn);		/* invoke watch procs */
	_IceFreeConnection (iceConn);
	return (IceClosedNow);
    }


    /*---------------------------------------------------------------

    ACTIONS:

    A = Invoke Watch Procedures
    B = Set free-asap bit
    C = Free connection
    D = Initialize shutdown negotiation
    N = do nothing


    ACTION TABLE:

    IO	       free-      dispatch   protocol   shutdown
    error      asap bit   level      refcount   negotiation     ACTION
    occurred   set        reached 0  reached 0

        0          0          0          0          0		N
        0          0          0          0          1		N
        0          0          0          1          0		AB
        0          0          0          1          1		N
        0          0          1          0          0		N
        0          0          1          0          1		N
        0          0          1          1          0		AC
        0          0          1          1          1		D
        0          1          0          0          0		N
        0          1          0          0          1		N
        0          1          0          1          0		N
        0          1          0          1          1		N
        0          1          1          0          0		C
        0          1          1          0          1		D
        0          1          1          1          0		C
        0          1          1          1          1		D
        1          0          0          0          0		AB
        1          0          0          0          1		AB
        1          0          0          1          0		AB
        1          0          0          1          1		AB
        1          0          1          0          0		AC
        1          0          1          0          1		AC
        1          0          1          1          0		AC
        1          0          1          1          1		AC
        1          1          0          0          0		N
        1          1          0          0          1		N
        1          1          0          1          0		N
        1          1          0          1          1		N
        1          1          1          0          0		C
        1          1          1          0          1		C
        1          1          1          1          0		C
        1          1          1          1          1		C

    ---------------------------------------------------------------*/

    if (iceConn->open_ref_count > 0)
	iceConn->open_ref_count--;

    refCountReachedZero = iceConn->open_ref_count == 0 &&
	iceConn->proto_ref_count == 0;

    status = IceConnectionInUse;

    if (!iceConn->free_asap && (!iceConn->io_ok ||
	(iceConn->io_ok && refCountReachedZero &&
	iceConn->skip_want_to_close)))
    {
	/*
	 * Invoke the watch procedures now.
	 */

	_IceConnectionClosed (iceConn);
	status = IceClosedNow;	     /* may be overwritten by IceClosedASAP */
    }

    if (!iceConn->free_asap && iceConn->dispatch_level != 0 &&
	(!iceConn->io_ok ||
	(iceConn->io_ok && refCountReachedZero &&
	iceConn->skip_want_to_close)))
    {
	/*
	 * Set flag so we free the connection as soon as possible.
	 */

	iceConn->free_asap = True;
	status = IceClosedASAP;
    }

    if (iceConn->io_ok && iceConn->dispatch_level == 0 &&
	!iceConn->skip_want_to_close && refCountReachedZero)
    {
	/*
	 * Initiate shutdown negotiation.
	 */

	IceSimpleMessage (iceConn, 0, ICE_WantToClose);
	IceFlush (iceConn);

	iceConn->want_to_close = 1;

	status = IceStartedShutdownNegotiation;
    }
    else if (iceConn->dispatch_level == 0 &&
	(!iceConn->io_ok || (iceConn->io_ok && iceConn->skip_want_to_close &&
	(iceConn->free_asap || (!iceConn->free_asap && refCountReachedZero)))))
    {
	/*
	 * Free the connection.
	 */

	_IceFreeConnection (iceConn);

	status = IceClosedNow;
    }

    return (status);
}



void
_IceFreeConnection (
	IceConn iceConn
)
{
    if (iceConn->listen_obj == NULL)
    {
	/*
	 * This iceConn was created with IceOpenConnection.
	 * We keep track of all open IceConn's, so we need
	 * to remove it from the list.
	 */

	int i;

	for (i = 0; i < _IceConnectionCount; i++)
	    if (_IceConnectionObjs[i] == iceConn)
		break;

	if (i < _IceConnectionCount)
	{
	    if (i < _IceConnectionCount - 1)
	    {
		_IceConnectionObjs[i] =
		    _IceConnectionObjs[_IceConnectionCount - 1];
		_IceConnectionStrings[i] =
		    _IceConnectionStrings[_IceConnectionCount - 1];
	    }

	    _IceConnectionCount--;
	}
    }

    if (iceConn->trans_conn)
	_IceTransClose (iceConn->trans_conn);

    free (iceConn->connection_string);
    free (iceConn->vendor);
    free (iceConn->release);
    free (iceConn->inbuf);
    free (iceConn->outbuf);
    free (iceConn->scratch);
    free (iceConn->process_msg_info);
    free (iceConn->connect_to_you);
    free (iceConn->protosetup_to_you);
    free (iceConn->connect_to_me);
    free (iceConn->protosetup_to_me);
    free (iceConn);
}




