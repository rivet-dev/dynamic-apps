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

int
IceRegisterForProtocolSetup (
	const char		*protocolName,
	const char		*vendor,
	const char		*release,
	int			versionCount,
	IcePoVersionRec		*versionRecs,
	int			authCount,
	const char	        **authNames,
	IcePoAuthProc		*authProcs,
	IceIOErrorProc		IOErrorProc
)
{
    _IcePoProtocol 	*p;
    int			opcodeRet, i;

    for (i = 1; i <= _IceLastMajorOpcode; i++)
	if (strcmp (protocolName, _IceProtocols[i - 1].protocol_name) == 0)
	{
	    if (_IceProtocols[i - 1].orig_client != NULL)
	    {
		/*
		 * We've already registered this protocol.
		 */

		return (i);
	    }
	    else
	    {
		break;
	    }
	}

    if (i <= _IceLastMajorOpcode)
    {
	p = _IceProtocols[i - 1].orig_client = malloc (sizeof(_IcePoProtocol));
	opcodeRet = i;
    }
    else if (_IceLastMajorOpcode == 255 ||
	versionCount < 1 ||
	strlen (protocolName) == 0)
    {
	return (-1);
    }
    else
    {
	_IceProtocols[_IceLastMajorOpcode].protocol_name =
	    strdup(protocolName);

	p = _IceProtocols[_IceLastMajorOpcode].orig_client =
	    malloc (sizeof (_IcePoProtocol));

	_IceProtocols[_IceLastMajorOpcode].accept_client = NULL;

	opcodeRet = ++_IceLastMajorOpcode;
    }

    p->vendor = strdup(vendor);
    p->release = strdup(release);

    p->version_count = versionCount;

    p->version_recs = malloc (versionCount * sizeof (IcePoVersionRec));
    memcpy (p->version_recs, versionRecs,
	versionCount * sizeof (IcePoVersionRec));

    if ((p->auth_count = authCount) > 0)
    {
	p->auth_names = malloc (authCount * sizeof (char *));

	p->auth_procs = malloc (authCount * sizeof (IcePoAuthProc));

	for (i = 0; i < authCount; i++)
	{
	    p->auth_names[i] = strdup(authNames[i]);
	    p->auth_procs[i] = authProcs[i];
	}
    }
    else
    {
	p->auth_names = NULL;
	p->auth_procs = NULL;
    }

    p->io_error_proc = IOErrorProc;

    return (opcodeRet);
}



int
IceRegisterForProtocolReply (
	const char			*protocolName,
	const char			*vendor,
	const char			*release,
	int				versionCount,
	IcePaVersionRec			*versionRecs,
	int				authCount,
	const char			**authNames,
	IcePaAuthProc			*authProcs,
	IceHostBasedAuthProc		hostBasedAuthProc,
	IceProtocolSetupProc		protocolSetupProc,
	IceProtocolActivateProc		protocolActivateProc,
	IceIOErrorProc			IOErrorProc
)
{
    _IcePaProtocol 	*p;
    int			opcodeRet, i;

    for (i = 1; i <= _IceLastMajorOpcode; i++)
	if (strcmp (protocolName, _IceProtocols[i - 1].protocol_name) == 0)
	{
	    if (_IceProtocols[i - 1].accept_client != NULL)
	    {
		/*
		 * We've already registered this protocol.
		 */

		return (i);
	    }
	    else
	    {
		break;
	    }
	}


    if (i <= _IceLastMajorOpcode)
    {
	p = _IceProtocols[i - 1].accept_client =
	    malloc (sizeof (_IcePaProtocol));
	opcodeRet = i;
    }
    else if (_IceLastMajorOpcode == 255 ||
	versionCount < 1 ||
	strlen (protocolName) == 0)
    {
	return (-1);
    }
    else
    {
	_IceProtocols[_IceLastMajorOpcode].protocol_name =
	    strdup(protocolName);

	_IceProtocols[_IceLastMajorOpcode].orig_client = NULL;

	p = _IceProtocols[_IceLastMajorOpcode].accept_client =
	    malloc (sizeof (_IcePaProtocol));

	opcodeRet = ++_IceLastMajorOpcode;
    }

    p->vendor = strdup(vendor);
    p->release = strdup(release);

    p->version_count = versionCount;

    p->version_recs = malloc (versionCount * sizeof (IcePaVersionRec));
    memcpy (p->version_recs, versionRecs,
	versionCount * sizeof (IcePaVersionRec));

    p->protocol_setup_proc = protocolSetupProc;
    p->protocol_activate_proc = protocolActivateProc;

    if ((p->auth_count = authCount) > 0)
    {
	p->auth_names = malloc (authCount * sizeof (char *));

	p->auth_procs = malloc (authCount * sizeof (IcePaAuthProc));

	for (i = 0; i < authCount; i++)
	{
	    p->auth_names[i] = strdup(authNames[i]);
	    p->auth_procs[i] = authProcs[i];
	}
    }
    else
    {
	p->auth_names = NULL;
	p->auth_procs = NULL;
    }

    p->host_based_auth_proc = hostBasedAuthProc;

    p->io_error_proc = IOErrorProc;

    return (opcodeRet);
}

