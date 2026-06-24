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
#include <X11/Xos.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

#include <time.h>
#ifndef X_NOT_POSIX
#include <unistd.h>
#else
#ifndef WIN32
extern unsigned	sleep ();
#else
#define link rename
#endif
#endif

static Status read_short (FILE *file, unsigned short *shortp);
static Status read_string (FILE *file, char **stringp);
static Status read_counted_string (FILE *file, unsigned short *countp, char **stringp);
static Status write_short (FILE *file, unsigned short s);
static Status write_string (FILE *file, const char *string);
static Status write_counted_string (FILE *file, unsigned short count, const char *string);



/*
 * The following routines are for manipulating the .ICEauthority file
 * These are utility functions - they are not part of the standard
 * ICE library specification.
 */

char *
IceAuthFileName (void)
{
    const char  *ICEauthority_name = ".ICEauthority";
    char    	*name;
    static char	*buf;
    static size_t bsize;
    size_t    	size;
#ifdef WIN32
#ifndef PATH_MAX
#define PATH_MAX 512
#endif
    char    	dir[PATH_MAX];
#endif

    if ((name = getenv ("ICEAUTHORITY")))
	return (name);

    /* If it's in the XDG_RUNTIME_DIR, don't use a dotfile */
    if ((name = getenv ("XDG_RUNTIME_DIR")))
	ICEauthority_name++;

    if (!name || !name[0])
	name = getenv ("HOME");

    if (!name || !name[0])
    {
#ifdef WIN32
    register char *ptr1;
    register char *ptr2;
    int len1 = 0, len2 = 0;

    if ((ptr1 = getenv("HOMEDRIVE")) && (ptr2 = getenv("HOMEDIR"))) {
	len1 = strlen (ptr1);
	len2 = strlen (ptr2);
    } else if ((ptr2 = getenv("USERNAME"))) {
	len1 = strlen (ptr1 = "/users/");
	len2 = strlen (ptr2);
    }
    if ((len1 + len2 + 1) < PATH_MAX) {
	snprintf (dir, sizeof(dir), "%s%s", ptr1, (ptr2) ? ptr2 : "");
	name = dir;
    }
    if (!name || !name[0])
#endif
	return (NULL);
    }

    /* Special case for "/".  We will add our own '/' later. */
    if (name[1] == '\0')
	name++;

    size = strlen (name) + 1 + strlen (ICEauthority_name) + 1;

    if (size > bsize)
    {

	free (buf);
	buf = malloc (size);
	if (!buf) {
	    bsize = 0;
	    return (NULL);
	}
	bsize = size;
    }

    snprintf (buf, bsize, "%s/%s", name, ICEauthority_name);

    return (buf);
}



int
IceLockAuthFile (
	const char *file_name,
	int	retries,
	int	timeout,
	long	dead
)
{
    char	creat_name[1025], link_name[1025];
    struct stat	statb;
    time_t	now;
    int		creat_fd = -1;

    if ((int) strlen (file_name) > 1022)
	return (IceAuthLockError);

    snprintf (creat_name, sizeof(creat_name), "%s-c", file_name);
    snprintf (link_name, sizeof(link_name), "%s-l", file_name);

    if (stat (creat_name, &statb) != -1)
    {
	now = time ((time_t *) 0);

	/*
	 * NFS may cause ctime to be before now, special
	 * case a 0 deadtime to force lock removal
	 */

	if (dead == 0 || now - statb.st_ctime > dead)
	{
	    unlink (creat_name);
	    unlink (link_name);
	}
    }

    while (retries > 0)
    {
	if (creat_fd == -1)
	{
	    creat_fd = creat (creat_name, 0666);

	    if (creat_fd == -1)
	    {
		if (errno != EACCES)
		    return (IceAuthLockError);
	    }
	    else
		close (creat_fd);
	}

	if (creat_fd != -1)
	{
	    if (link (creat_name, link_name) != -1)
		return (IceAuthLockSuccess);

	    if (errno == ENOENT)
	    {
		creat_fd = -1;	/* force re-creat next time around */
		continue;
	    }

	    if (errno != EEXIST)
		return (IceAuthLockError);
	}

	sleep ((unsigned) timeout);
	--retries;
    }

    return (IceAuthLockTimeout);
}



void
IceUnlockAuthFile (
	const char	*file_name
)
{
#ifndef WIN32
    char	creat_name[1025];
#endif
    char	link_name[1025];

    if ((int) strlen (file_name) > 1022)
	return;

#ifndef WIN32
    snprintf (creat_name, sizeof(creat_name), "%s-c", file_name);
    unlink (creat_name);
#endif
    snprintf (link_name, sizeof(link_name), "%s-l", file_name);
    unlink (link_name);
}



IceAuthFileEntry *
IceReadAuthFileEntry (
	FILE	*auth_file
)
{
    IceAuthFileEntry   	local;
    IceAuthFileEntry   	*ret;

    local.protocol_name = NULL;
    local.protocol_data = NULL;
    local.network_id = NULL;
    local.auth_name = NULL;
    local.auth_data = NULL;

    if (!read_string (auth_file, &local.protocol_name))
	return (NULL);

    if (!read_counted_string (auth_file,
	&local.protocol_data_length, &local.protocol_data))
	goto bad;

    if (!read_string (auth_file, &local.network_id))
	goto bad;

    if (!read_string (auth_file, &local.auth_name))
	goto bad;

    if (!read_counted_string (auth_file,
	&local.auth_data_length, &local.auth_data))
	goto bad;

    if (!(ret = malloc (sizeof (IceAuthFileEntry))))
	goto bad;

    *ret = local;

    return (ret);

 bad:

    free (local.protocol_name);
    free (local.protocol_data);
    free (local.network_id);
    free (local.auth_name);
    free (local.auth_data);

    return (NULL);
}



void
IceFreeAuthFileEntry (
	IceAuthFileEntry	*auth
)
{
    if (auth)
    {
	free (auth->protocol_name);
	free (auth->protocol_data);
	free (auth->network_id);
	free (auth->auth_name);
	free (auth->auth_data);
	free (auth);
    }
}



Status
IceWriteAuthFileEntry (
	FILE			*auth_file,
	IceAuthFileEntry	*auth
)
{
    if (!write_string (auth_file, auth->protocol_name))
	return (0);

    if (!write_counted_string (auth_file,
	auth->protocol_data_length, auth->protocol_data))
	return (0);

    if (!write_string (auth_file, auth->network_id))
	return (0);

    if (!write_string (auth_file, auth->auth_name))
	return (0);

    if (!write_counted_string (auth_file,
	auth->auth_data_length, auth->auth_data))
	return (0);

    return (1);
}



IceAuthFileEntry *
IceGetAuthFileEntry (
	const char	*protocol_name,
	const char	*network_id,
	const char	*auth_name
)
{
    FILE    		*auth_file;
    char    		*filename;
    IceAuthFileEntry    *entry;

    if (!(filename = IceAuthFileName ()))
	return (NULL);

    if (access (filename, R_OK) != 0)		/* checks REAL id */
	return (NULL);

    if (!(auth_file = fopen (filename, "rb")))
	return (NULL);

    for (;;)
    {
	if (!(entry = IceReadAuthFileEntry (auth_file)))
	    break;

	if (strcmp (protocol_name, entry->protocol_name) == 0 &&
	    strcmp (network_id, entry->network_id) == 0 &&
            strcmp (auth_name, entry->auth_name) == 0)
	{
	    break;
	}

	IceFreeAuthFileEntry (entry);
    }

    fclose (auth_file);

    return (entry);
}



/*
 * local routines
 */

static Status
read_short (FILE *file, unsigned short *shortp)
{
    unsigned char   file_short[2];

    if (fread (file_short, sizeof (file_short), 1, file) != 1)
	return (0);

    *shortp = file_short[0] * 256 + file_short[1];
    return (1);
}


static Status
read_string (FILE *file, char **stringp)

{
    unsigned short  len;
    char	    *data;

    if (!read_short (file, &len))
	return (0);

    data = malloc ((unsigned) len + 1);

    if (!data)
	    return (0);

    if (len != 0)
    {
	if (fread (data, sizeof (char), len, file) != len)
	{
	    free (data);
	    return (0);
	}

    }
    data[len] = '\0';

    *stringp = data;

    return (1);
}


static Status
read_counted_string (FILE *file, unsigned short	*countp, char **stringp)
{
    unsigned short  len;
    char	    *data;

    if (!read_short (file, &len))
	return (0);

    if (len == 0)
    {
	data = NULL;
    }
    else
    {
    	data = malloc ((unsigned) len);

    	if (!data)
	    return (0);

	if (fread (data, sizeof (char), len, file) != len)
	{
	    free (data);
	    return (0);
    	}
    }

    *stringp = data;
    *countp = len;

    return (1);
}


static Status
write_short (FILE *file, unsigned short s)
{
    unsigned char   file_short[2];

    file_short[0] = (s & (unsigned) 0xff00) >> 8;
    file_short[1] = s & 0xff;

    if (fwrite (file_short, sizeof (file_short), 1, file) != 1)
	return (0);

    return (1);
}


static Status
write_string (FILE *file, const char *string)
{
    size_t count = strlen (string);

    if (count > USHRT_MAX)
	return (0);

    return write_counted_string (file, (unsigned short) count, string);
}


static Status
write_counted_string (FILE *file, unsigned short count, const char *string)
{
    if (!write_short (file, count))
	return (0);

    if (fwrite (string, sizeof (char), count, file) != count)
	return (0);

    return (1);
}
