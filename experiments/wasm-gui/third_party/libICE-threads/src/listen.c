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

Author: Ralph Mor,  X Consortium
******************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/ICE/ICElib.h>
#include "ICElibint.h"
#include <X11/Xtrans/Xtrans.h>
#include <stdio.h>


Status
IceListenForConnections (
	int		*countRet,
	IceListenObj	**listenObjsRet,
	int		errorLength,
	char		*errorStringRet
)
{
    struct _IceListenObj	*listenObjs;
    char			*networkId;
    int				transCount, partial, i, j;
    Status			status = 1;
    XtransConnInfo		*transConns = NULL;


    if ((_IceTransMakeAllCOTSServerListeners (NULL, &partial,
	&transCount, &transConns) < 0) || (transCount < 1))
    {
	*listenObjsRet = NULL;
	*countRet = 0;

	if (errorStringRet && errorLength > 0) {
	    strncpy (errorStringRet,
		"Cannot establish any listening sockets", errorLength);
	    errorStringRet[errorLength - 1] = '\0';
	}

	return (0);
    }

    if ((listenObjs = malloc (transCount * sizeof (struct _IceListenObj))) == NULL)
    {
	for (i = 0; i < transCount; i++)
	    _IceTransClose (transConns[i]);
	free (transConns);
	return (0);
    }

    *countRet = 0;

    for (i = 0; i < transCount; i++)
    {
	_IceTransSetOption(transConns[i], TRANS_CLOSEONEXEC, 1);

	networkId = _IceTransGetMyNetworkId (transConns[i]);

	if (networkId)
	{
	    listenObjs[*countRet].trans_conn = transConns[i];
	    listenObjs[*countRet].network_id = networkId;

	    (*countRet)++;
	}
    }

    if (*countRet == 0)
    {
	*listenObjsRet = NULL;

	if (errorStringRet && errorLength > 0) {
	    strncpy (errorStringRet,
		"Cannot establish any listening sockets", errorLength);
	    errorStringRet[errorLength - 1] = '\0';
	}

	status = 0;
    }
    else
    {
	*listenObjsRet = malloc (*countRet * sizeof (IceListenObj));

	if (*listenObjsRet == NULL)
	{
	    if (errorStringRet && errorLength > 0) {
		strncpy (errorStringRet, "Malloc failed", errorLength);
		errorStringRet[errorLength - 1] = '\0';
	    }

	    status = 0;
	}
	else
	{
	    for (i = 0; i < *countRet; i++)
	    {
		(*listenObjsRet)[i] = malloc (sizeof (struct _IceListenObj));

		if ((*listenObjsRet)[i] == NULL)
		{
		    if (errorStringRet && errorLength > 0) {
			strncpy (errorStringRet, "Malloc failed", errorLength);
			errorStringRet[errorLength - 1] = '\0';
		    }

		    for (j = 0; j < i; j++)
			free ((*listenObjsRet)[j]);

		    free (*listenObjsRet);
		    *listenObjsRet = NULL;

		    status = 0;
		    break;
		}
		else
		{
		    *((*listenObjsRet)[i]) = listenObjs[i];
		}
	    }
	}
    }

    if (status == 1)
    {
	if (errorStringRet && errorLength > 0)
	    *errorStringRet = '\0';

	for (i = 0; i < *countRet; i++)
	{
	    (*listenObjsRet)[i]->host_based_auth_proc = NULL;
	}
    }
    else
    {
	for (i = 0; i < transCount; i++)
	    _IceTransClose (transConns[i]);
    }

    free (listenObjs);
    free (transConns);

    return (status);
}



int
IceGetListenConnectionNumber (
	IceListenObj listenObj
)
{
    return (_IceTransGetConnectionNumber (listenObj->trans_conn));
}



char *
IceGetListenConnectionString (
	IceListenObj listenObj
)
{
    return strdup(listenObj->network_id);
}



char *
IceComposeNetworkIdList (
	int		count,
	IceListenObj	*listenObjs
)
{
    char *list;
    int len = 0;
    int i;

    if (count < 1 || listenObjs == NULL)
	return (NULL);

    for (i = 0; i < count; i++)
	len += (strlen (listenObjs[i]->network_id) + 1);

    list = malloc (len);

    if (list == NULL)
	return (NULL);
    else
    {
	int doneCount = 0;

	list[0] = '\0';

	for (i = 0; i < count; i++)
	{
	    if (_IceTransIsLocal (listenObjs[i]->trans_conn))
	    {
		strcat (list, listenObjs[i]->network_id);
		doneCount++;
		if (doneCount < count)
		    strcat (list, ",");
	    }
	}

	if (doneCount < count)
	{
	    for (i = 0; i < count; i++)
	    {
		if (!_IceTransIsLocal (listenObjs[i]->trans_conn))
		{
		    strcat (list, listenObjs[i]->network_id);
		    doneCount++;
		    if (doneCount < count)
			strcat (list, ",");
		}
	    }
	}

	return (list);
    }
}



void
IceFreeListenObjs (
	int	     count,
	IceListenObj *listenObjs
)
{
    int i;

    for (i = 0; i < count; i++)
    {
	free (listenObjs[i]->network_id);
	_IceTransClose (listenObjs[i]->trans_conn);
	free (listenObjs[i]);
    }

    free (listenObjs);
}



/*
 * Allow host based authentication for the ICE Connection Setup.
 * Do not confuse with the host based authentication callbacks that
 * can be set up in IceRegisterForProtocolReply.
 */

void
IceSetHostBasedAuthProc (
	IceListenObj		listenObj,
	IceHostBasedAuthProc	hostBasedAuthProc
)
{
    listenObj->host_based_auth_proc = hostBasedAuthProc;
}
