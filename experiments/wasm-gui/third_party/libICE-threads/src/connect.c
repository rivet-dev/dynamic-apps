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
#include "globals.h"

static XtransConnInfo ConnectToPeer(char *networkIdsList,
				    char **actualConnectionRet);

IceConn
IceOpenConnection (
	char 	   *networkIdsList,
	IcePointer context,
	Bool 	   mustAuthenticate,
	int  	   majorOpcodeCheck,
	int  	   errorLength,
	char 	   *errorStringRet
)
{
    IceConn			iceConn;
    int				extra, i, j;
    int		       		endian;
    Bool			gotReply, ioErrorOccured;
    unsigned long		setup_sequence;
    iceByteOrderMsg		*pByteOrderMsg;
    iceConnectionSetupMsg	*pSetupMsg;
    char			*pData;
    IceReplyWaitInfo 		replyWait;
    _IceReply		 	reply;
    int				authUsableCount;
    int				authUsableFlags[MAX_ICE_AUTH_NAMES];
    int				authIndices[MAX_ICE_AUTH_NAMES];

    if (errorStringRet && errorLength > 0)
	*errorStringRet = '\0';

    if (networkIdsList == NULL || *networkIdsList == '\0')
    {
	if (errorStringRet && errorLength > 0) {
	    strncpy (errorStringRet,
		"networkIdsList argument is NULL", errorLength);
	    errorStringRet[errorLength - 1] = '\0';
	}
	return (NULL);
    }

    /*
     * Check to see if we can use a previously created ICE connection.
     *
     * If iceConn->want_to_close is True, or iceConn->free_asap is True,
     * we can not use the iceConn.
     *
     * If 'context' is non-NULL, we will only use a previously opened ICE
     * connection if the specified 'context' is equal to the context
     * associated with the ICE connection, or if the context associated
     * with the ICE connection is NULL.
     *
     * If 'majorOpcodeCheck' is non-zero, it will contain a protocol major
     * opcode that we should make sure is not already active on the ICE
     * connection.  Some clients will want two separate connections for the
     * same protocol to the same destination client.
     */

    for (i = 0; i < _IceConnectionCount; i++)
    {
	char *strptr;
	if ((strptr = (char *) strstr (
	    networkIdsList, _IceConnectionStrings[i])) != NULL)
	{
	    char ch = *(strptr + strlen (_IceConnectionStrings[i]));
	    if (ch == ',' || ch == '\0')
	    {
		/*
		 * OK, we found a connection.  Make sure we can reuse it.
		 */

		IceConn iConn = _IceConnectionObjs[i];

		if (iConn->want_to_close || iConn->free_asap ||
		    (context && iConn->context &&
		     iConn->context != context))
		{
		    /* force a new connection to be created */
		    break;
		}

		if (majorOpcodeCheck)
		{
		    for (j = iConn->his_min_opcode;
		        j <= iConn->his_max_opcode; j++)
		    {
			if (iConn->process_msg_info[
			    j - iConn->his_min_opcode].in_use &&
			    iConn->process_msg_info[
			    j - iConn->his_min_opcode].my_opcode ==
			    majorOpcodeCheck)
			    break;
		    }

		    if (j <= iConn->his_max_opcode ||
			(iConn->protosetup_to_you &&
			iConn->protosetup_to_you->my_opcode ==
			majorOpcodeCheck))
		    {
			/* force a new connection to be created */
			break;
		    }
		}

		iConn->open_ref_count++;
		if (context && !iConn->context)
		    iConn->context = context;
		return (iConn);
	    }
	}
    }

    if ((iceConn = malloc (sizeof (struct _IceConn))) == NULL)
    {
	if (errorStringRet && errorLength > 0) {
	    strncpy (errorStringRet, "Can't malloc", errorLength);
	    errorStringRet[errorLength - 1] = '\0';
	}
	return (NULL);
    }


    /*
     * Open a network connection with the peer client.
     */

    if ((iceConn->trans_conn = ConnectToPeer (networkIdsList,
	&iceConn->connection_string)) == NULL)
    {
	free (iceConn);
	if (errorStringRet && errorLength > 0) {
	    strncpy (errorStringRet, "Could not open network socket", errorLength);
	    errorStringRet[errorLength - 1] = '\0';
	}
	return (NULL);
    }

    /*
     * Set close-on-exec so that programs that fork() don't get confused.
     */

    _IceTransSetOption (iceConn->trans_conn, TRANS_CLOSEONEXEC, 1);

    iceConn->listen_obj = NULL;

    iceConn->connection_status = IceConnectPending;
    iceConn->io_ok = True;
    iceConn->dispatch_level = 0;
    iceConn->context = context;
    iceConn->my_ice_version_index = 0;
    iceConn->send_sequence = 0;
    iceConn->receive_sequence = 0;

    iceConn->vendor = NULL;
    iceConn->release = NULL;
    iceConn->outbuf = NULL;

    iceConn->scratch = NULL;
    iceConn->scratch_size = 0;

    iceConn->process_msg_info = NULL;

    iceConn->connect_to_you = NULL;
    iceConn->protosetup_to_you = NULL;

    iceConn->connect_to_me = NULL;
    iceConn->protosetup_to_me = NULL;

    if ((iceConn->inbuf = iceConn->inbufptr = malloc (ICE_INBUFSIZE)) == NULL)
    {
	_IceFreeConnection (iceConn);
	if (errorStringRet && errorLength > 0) {
	    strncpy (errorStringRet, "Can't malloc", errorLength);
	    errorStringRet[errorLength - 1] = '\0';
	}
	return (NULL);
    }

    iceConn->inbufmax = iceConn->inbuf + ICE_INBUFSIZE;

    if ((iceConn->outbuf = iceConn->outbufptr = calloc (1, ICE_OUTBUFSIZE)) == NULL)
    {
	_IceFreeConnection (iceConn);
	if (errorStringRet && errorLength > 0) {
	    strncpy (errorStringRet, "Can't malloc", errorLength);
	    errorStringRet[errorLength - 1] = '\0';
	}
	return (NULL);
    }

    iceConn->outbufmax = iceConn->outbuf + ICE_OUTBUFSIZE;

    iceConn->open_ref_count = 1;
    iceConn->proto_ref_count = 0;

    iceConn->skip_want_to_close = False;
    iceConn->want_to_close = False;
    iceConn->free_asap = False;

    iceConn->saved_reply_waits = NULL;
    iceConn->ping_waits = NULL;

    iceConn->connect_to_you = malloc (sizeof (_IceConnectToYouInfo));
    if (iceConn->connect_to_you == NULL)
    {
	_IceFreeConnection (iceConn);
	if (errorStringRet && errorLength > 0) {
	    strncpy (errorStringRet, "Can't malloc", errorLength);
	    errorStringRet[errorLength - 1] = '\0';
	}
	return (NULL);
    }
    iceConn->connect_to_you->auth_active = 0;

    /*
     * Send our byte order.
     */

    IceGetHeader (iceConn, 0, ICE_ByteOrder,
	SIZEOF (iceByteOrderMsg), iceByteOrderMsg, pByteOrderMsg);

    endian = 1;
    if (*(char *) &endian)
	pByteOrderMsg->byteOrder = IceLSBfirst;
    else
	pByteOrderMsg->byteOrder = IceMSBfirst;

    IceFlush (iceConn);


    /*
     * Now read the ByteOrder message from the other client.
     * iceConn->swap should be set to the appropriate boolean
     * value after the call to IceProcessMessages.
     */

    iceConn->waiting_for_byteorder = True;

    ioErrorOccured = False;
    while (iceConn->waiting_for_byteorder == True && !ioErrorOccured)
    {
	ioErrorOccured = (IceProcessMessages (
	    iceConn, NULL, NULL) == IceProcessMessagesIOError);
    }

    if (ioErrorOccured)
    {
	_IceFreeConnection (iceConn);
	if (errorStringRet && errorLength > 0) {
	    strncpy (errorStringRet, "IO error occurred opening connection",
		 errorLength);
	    errorStringRet[errorLength - 1] = '\0';
	}
	return (NULL);
    }

    if (iceConn->connection_status == IceConnectRejected)
    {
	/*
	 * We failed to get the required ByteOrder message.
	 */

	_IceFreeConnection (iceConn);
	if (errorStringRet && errorLength > 0) {
	    strncpy (errorStringRet,
		"Internal error - did not receive the expected ByteOrder "
		"message", errorLength);
	    errorStringRet[errorLength - 1] = '\0';
	}
	return (NULL);
    }


    /*
     * Determine which authentication methods are available for
     * the Connection Setup authentication.
     */

    _IceGetPoValidAuthIndices (
	"ICE", iceConn->connection_string,
	_IceAuthCount, _IceAuthNames, &authUsableCount, authIndices);

    for (i = 0; i < _IceAuthCount; i++)
    {
	authUsableFlags[i] = 0;
	for (j = 0; j < authUsableCount && !authUsableFlags[i]; j++)
	    authUsableFlags[i] = (authIndices[j] == i);
    }


    /*
     * Now send a Connection Setup message.
     */

    extra = STRING_BYTES (IceVendorString) + STRING_BYTES (IceReleaseString);

    for (i = 0; i < _IceAuthCount; i++)
	if (authUsableFlags[i])
	{
	    extra += STRING_BYTES (_IceAuthNames[i]);
	}

    extra += (_IceVersionCount * 4);

    IceGetHeaderExtra (iceConn, 0, ICE_ConnectionSetup,
	SIZEOF (iceConnectionSetupMsg), WORD64COUNT (extra),
	iceConnectionSetupMsg, pSetupMsg, pData);

    setup_sequence = iceConn->send_sequence;

    pSetupMsg->versionCount = _IceVersionCount;
    pSetupMsg->authCount = authUsableCount;
    pSetupMsg->mustAuthenticate = mustAuthenticate;

    if (_X_LIKELY(pData != NULL)) {
	STORE_STRING (pData, IceVendorString);
	STORE_STRING (pData, IceReleaseString);

	for (i = 0; i < _IceAuthCount; i++)
	{
	    if (authUsableFlags[i])
	    {
		STORE_STRING (pData, _IceAuthNames[i]);
	    }
	}

	for (i = 0; i < _IceVersionCount; i++)
	{
	    STORE_CARD16 (pData, _IceVersions[i].major_version);
	    STORE_CARD16 (pData, _IceVersions[i].minor_version);
	}
    }
    else {
	SEND_STRING (iceConn, IceVendorString);
	SEND_STRING (iceConn, IceReleaseString);

	for (i = 0; i < _IceAuthCount; i++)
	{
	    if (authUsableFlags[i])
	    {
		SEND_STRING (iceConn, _IceAuthNames[i]);
	    }
	}

	for (i = 0; i < _IceVersionCount; i++)
	{
	    CARD16 v;
	    v = _IceVersions[i].major_version;
	    IceWriteData16 (iceConn, 2, &v);
	    v = _IceVersions[i].minor_version;
	    IceWriteData16 (iceConn, 2, &v);
	}
    }
    IceFlush (iceConn);

    /*
     * Process messages until we get a Connection Reply or an Error Message.
     * Authentication will take place behind the scenes.
     */

    replyWait.sequence_of_request = setup_sequence;
    replyWait.major_opcode_of_request = 0;
    replyWait.minor_opcode_of_request = ICE_ConnectionSetup;
    replyWait.reply = (IcePointer) &reply;

    gotReply = False;
    ioErrorOccured = False;

    while (!gotReply && !ioErrorOccured)
    {
	ioErrorOccured = (IceProcessMessages (
	    iceConn, &replyWait, &gotReply) == IceProcessMessagesIOError);

	if (ioErrorOccured)
	{
	    if (errorStringRet && errorLength > 0) {
		strncpy (errorStringRet, "IO error occurred opening connection",
		    errorLength);
		errorStringRet[errorLength - 1] = '\0';
	    }
	    _IceFreeConnection (iceConn);
	    iceConn = NULL;
	}
	else if (gotReply)
	{
	    if (reply.type == ICE_CONNECTION_REPLY)
	    {
		if (reply.connection_reply.version_index >= _IceVersionCount)
		{
		    if (errorStringRet && errorLength > 0) {
			strncpy (errorStringRet,
			    "Got a bad version index in the Connection Reply",
			    errorLength);
			errorStringRet[errorLength - 1] = '\0';
		    }

		    free (reply.connection_reply.vendor);
		    free (reply.connection_reply.release);
		    _IceFreeConnection (iceConn);
		    iceConn = NULL;
		}
		else
		{
		    iceConn->my_ice_version_index =
			reply.connection_reply.version_index;
		    iceConn->vendor = reply.connection_reply.vendor;
		    iceConn->release = reply.connection_reply.release;

		    _IceConnectionObjs[_IceConnectionCount] = iceConn;
		    _IceConnectionStrings[_IceConnectionCount] =
			iceConn->connection_string;
		    _IceConnectionCount++;

		    free (iceConn->connect_to_you);
		    iceConn->connect_to_you = NULL;

		    iceConn->connection_status = IceConnectAccepted;
		}
	    }
	    else /* reply.type == ICE_CONNECTION_ERROR */
	    {
		/* Connection failed */

		if (errorStringRet && errorLength > 0) {
		    strncpy (errorStringRet,
			reply.connection_error.error_message, errorLength);
		    errorStringRet[errorLength - 1] = '\0';
		}

		free (reply.connection_error.error_message);

		_IceFreeConnection (iceConn);
		iceConn = NULL;
	    }
	}
    }

    if (iceConn && _IceWatchProcs)
    {
	/*
	 * Notify the watch procedures that an iceConn was opened.
	 */

	_IceConnectionOpened (iceConn);
    }

    return (iceConn);
}



IcePointer
IceGetConnectionContext (
	IceConn    iceConn
)
{
    return (iceConn->context);
}



/* ------------------------------------------------------------------------- *
 *                            local routines                                 *
 * ------------------------------------------------------------------------- */

#define ICE_CONNECTION_RETRIES 5


static XtransConnInfo
ConnectToPeer (char *networkIdsList, char **actualConnectionRet)
{
    char addrbuf[256];
    char* address;
    char *ptr, *endptr, *delim;
    int  madeConnection = 0;
    size_t  len;
    int  retry, connect_stat;
    size_t  address_size;
    XtransConnInfo trans_conn = NULL;

    *actualConnectionRet = NULL;

    ptr = networkIdsList;
    len = strlen (networkIdsList);
    endptr = networkIdsList + len;

    if (len < sizeof addrbuf)
    {
       address = addrbuf;
       address_size = 256;
    }
    else
    {
       address = malloc (len + 1);
       address_size = len;
    }

    while (ptr < endptr && !madeConnection)
    {
	if ((delim = (char *) strchr (ptr, ',')) == NULL)
	    delim = endptr;

	len = delim - ptr;
	if (len > address_size - 1)
	    len = address_size - 1;
	strncpy (address, ptr, len);
	address[len] = '\0';

	ptr = delim + 1;

	for (retry = ICE_CONNECTION_RETRIES; retry >= 0; retry--)
	{
	    if ((trans_conn = _IceTransOpenCOTSClient (address)) == NULL)
	    {
		break;
	    }

	    if ((connect_stat = _IceTransConnect (trans_conn, address)) < 0)
	    {
		_IceTransClose (trans_conn);
		trans_conn = NULL;

		if (connect_stat == TRANS_TRY_CONNECT_AGAIN)
		{
		    sleep(1);
		    continue;
		}
		else
		    break;
	    }
	    else
	    {
		madeConnection = 1;
		break;
	    }
	}
    }

    if (madeConnection)
    {
	/*
	 * We need to return the actual network connection string
	 */

	*actualConnectionRet = strdup(address);

	/*
	 * Return the file descriptor
	 */
    }
    else trans_conn = NULL;

    if (address != addrbuf) free (address);

    return trans_conn;
}
