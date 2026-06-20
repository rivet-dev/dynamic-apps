/*

Copyright 1996, 1998  The Open Group

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


/* Author: Ralph Mor, X Consortium */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/ICE/ICElib.h>
#include "ICElibint.h"
#include <X11/Xtrans/Xtrans.h>
#include <stdio.h>


Status
IceListenForWellKnownConnections (
	char		*port,
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


    if ((_IceTransMakeAllCOTSServerListeners (port, &partial,
	&transCount, &transConns) < 0) || (transCount < 1))
    {
	*listenObjsRet = NULL;
	*countRet = 0;

	if (errorStringRet && errorLength > 0) {
            strncpy (errorStringRet,
		"Cannot establish any listening sockets", errorLength);
	    errorStringRet[errorLength - 1] = '\0';
	}

	free (transConns);
	return (0);
    }

    listenObjs = calloc (transCount, sizeof (struct _IceListenObj));
    if (listenObjs == NULL)
    {
	for (i = 0; i < transCount; i++)
	    _IceTransClose (transConns[i]);
	free (transConns);

	strncpy (errorStringRet, "Malloc failed", errorLength);
	return (0);
    }

    *countRet = 0;

    for (i = 0; i < transCount; i++)
    {
	networkId = (char *)_IceTransGetMyNetworkId (transConns[i]);

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
