/**
 * @file grab.c
 * @author Joe Wingbermuehle
 * @date 2013
 *
 * @brief Functions for managing server grabs.
 *
 */

#include "jwm.h"
#include "main.h"

static unsigned int grabCount = 0;

/** Grab the server and sync. */
void GrabServer(void)
{
   if(grabCount == 0) {
      JXGrabServer(display);
      JXSync(display, False);
   }
   grabCount += 1;
}

/** Ungrab the server. */
void UngrabServer(void)
{
   Assert(grabCount > 0);
   grabCount -= 1;
   if(grabCount == 0) {
      JXUngrabServer(display);
   }
}

