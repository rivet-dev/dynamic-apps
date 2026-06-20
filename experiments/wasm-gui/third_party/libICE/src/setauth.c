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
#include <X11/ICE/ICEutil.h>


/*
 * IceSetPaAuthData is not a standard part of ICElib, it is specific
 * to the sample implementation.
 *
 * For the client that initiates a Protocol Setup, we look in the
 * .ICEauthority file to get authentication data.
 *
 * For the client accepting the Protocol Setup, we get the data
 * from an in-memory database of authentication data (set by the
 * application calling IceSetPaAuthData).  We have to get the data
 * from memory because getting it directly from the .ICEauthority
 * file is not secure - someone can just modify the contents of the
 * .ICEauthority file behind our back.
 */

int		 _IcePaAuthDataEntryCount = 0;
IceAuthDataEntry _IcePaAuthDataEntries[ICE_MAX_AUTH_DATA_ENTRIES];


void
IceSetPaAuthData (
	int			numEntries,
	IceAuthDataEntry	*entries
)
{
    /*
     * _IcePaAuthDataEntries should really be a linked list.
     * On my list of TO DO stuff.
     */

    int i, j;

    for (i = 0; i < numEntries; i++)
    {
	for (j = 0; j < _IcePaAuthDataEntryCount; j++)
	    if (strcmp (entries[i].protocol_name,
		_IcePaAuthDataEntries[j].protocol_name) == 0 &&
                strcmp (entries[i].network_id,
		_IcePaAuthDataEntries[j].network_id) == 0 &&
                strcmp (entries[i].auth_name,
		_IcePaAuthDataEntries[j].auth_name) == 0)
		break;

	if (j < _IcePaAuthDataEntryCount)
	{
	    free (_IcePaAuthDataEntries[j].protocol_name);
	    free (_IcePaAuthDataEntries[j].network_id);
	    free (_IcePaAuthDataEntries[j].auth_name);
	    free (_IcePaAuthDataEntries[j].auth_data);
	}
	else
	{
	    _IcePaAuthDataEntryCount++;
	}

	_IcePaAuthDataEntries[j].protocol_name
	    = strdup(entries[i].protocol_name);

	_IcePaAuthDataEntries[j].network_id
	    = strdup(entries[i].network_id);

	_IcePaAuthDataEntries[j].auth_name
            = strdup(entries[i].auth_name);

	_IcePaAuthDataEntries[j].auth_data_length =
            entries[i].auth_data_length;
	_IcePaAuthDataEntries[j].auth_data = malloc (
            entries[i].auth_data_length);
	memcpy (_IcePaAuthDataEntries[j].auth_data,
            entries[i].auth_data, entries[i].auth_data_length);
    }
}
