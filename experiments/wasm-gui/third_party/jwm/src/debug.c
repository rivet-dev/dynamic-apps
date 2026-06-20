/**
 * @file debug.h
 * @author Joe Wingbermuehle
 * @date 2003-2006
 *
 * @brief Debug functions.
 *
 */

#include "debug.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/** Emit a message (if compiled with -DDEBUG). */
void Debug(const char *str, ...)
{
#ifdef DEBUG

   va_list ap;
   va_start(ap, str);
   Assert(str);
   fprintf(stderr, "DEBUG: ");
   vfprintf(stderr, str, ap);
   fprintf(stderr, "\n");
   va_end(ap);

#endif /* DEBUG */
}

#ifdef DEBUG

#define CHECKPOINT_LIST_SIZE 8

typedef struct MemoryType {
   const char *file;
   unsigned int line;
   size_t size;
   char *pointer;
   struct MemoryType *next;
} MemoryType;

static MemoryType *allocations = NULL;

static const char *checkpointFile[CHECKPOINT_LIST_SIZE];
static unsigned int checkpointLine[CHECKPOINT_LIST_SIZE];
static unsigned int checkpointOffset;

/** Start the debugger. */
void DEBUG_StartDebug(const char *file, unsigned int line)
{
   unsigned int x;
   Debug("%s[%u]: debug mode started", file, line);
   checkpointOffset = 0;
   for(x = 0; x < CHECKPOINT_LIST_SIZE; x++) {
      checkpointFile[x] = NULL;
      checkpointLine[x] = 0;
   }
}

/** Stop the debugger. */
void DEBUG_StopDebug(const char *file, unsigned int line)
{
   Debug("%s[%u]: debug mode stopped", file, line);
   if(allocations) {
      MemoryType *mp;
      unsigned int count = 0;
      Debug("MEMORY: memory leaks follow");
      for(mp = allocations; mp; mp = mp->next) {
         Debug("        %u bytes in %s at line %u",
            mp->size, mp->file, mp->line);
         ++count;
      }
      if(count == 1) {
         Debug("MEMORY: 1 memory leak");
      } else {
         Debug("MEMORY: %u memory leaks", count);
      }
   } else {
      Debug("MEMORY: no memory leaks");
   }
}

/** Set a checkpoint. */
void DEBUG_SetCheckpoint(const char *file, unsigned int line)
{
   checkpointFile[checkpointOffset] = file;
   checkpointLine[checkpointOffset] = line;
   checkpointOffset = (checkpointOffset + 1) % CHECKPOINT_LIST_SIZE;
}

/** Display the location of the last checkpoint. */
void DEBUG_ShowCheckpoint(void)
{
   unsigned int x, offset;
   Debug("CHECKPOINT LIST (oldest)");
   offset = checkpointOffset;
   for(x = 0; x < CHECKPOINT_LIST_SIZE; x++) {
      if(checkpointFile[offset]) {
         Debug("   %s[%u]", checkpointFile[offset], checkpointLine[offset]);
      }
      offset = (offset + 1) % CHECKPOINT_LIST_SIZE;
   }
   Debug("END OF CHECKPOINT LIST (most recent)");
}

/** Allocate memory and log. */
void *DEBUG_Allocate(size_t size, const char *file, unsigned int line)
{
   MemoryType *mp;
   mp = (MemoryType*)malloc(sizeof(MemoryType));
   Assert(mp);
   mp->file = file;
   mp->line = line;
   mp->size = size;
   mp->pointer = malloc(size + sizeof(char) + 8);
   if(!mp->pointer) {
      Debug("MEMORY: %s[%u]: Memory allocation failed (%d bytes)",
            file, line, (int)size);
      Assert(0);
   }

   /* Make uninitialized accesses easy to find. */
   memset(mp->pointer, 85, size);

   /* Canary value for buffer underflow/overflow checking. */
   mp->pointer[7] = 42;
   mp->pointer[size + 8] = 42;

   mp->next = allocations;
   allocations = mp;
   return mp->pointer + 8;
}

/** Reallocate memory and log. */
void *DEBUG_Reallocate(void *ptr, size_t size,
                       const char *file,
                       unsigned int line)
{
   MemoryType *mp;
   if(!ptr) {
      Debug("MEMORY: %s[%u]: Attempt to reallocate NULL pointer. "
            "Calling Allocate...", file, line);
      return DEBUG_Allocate(size, file, line);
   } else {
      char *cptr = (char*)ptr - 8;
      for(mp = allocations; mp; mp = mp->next) {
         if(mp->pointer == cptr) {
            if(cptr[mp->size + 8] != 42) {
               Debug("MEMORY: %s[%u]: The canary is dead (overflow).",
                     file, line);
            }
            if(cptr[7] != 42) {
               Debug("MEMORY: %s[%u]: The canary is dead (underflow).",
                     file, line);
            }
            mp->file = file;
            mp->line = line;
            mp->size = size;
            mp->pointer = realloc(cptr, size + sizeof(char) + 8);
            if(!mp->pointer) {
               Debug("MEMORY: %s[%u]: Failed to reallocate %d bytes.",
                     file, line, (int)size);
               Assert(0);
            }
            mp->pointer[7] = 42;
            mp->pointer[size + 8] = 42;
            return mp->pointer + 8;
         }
      }

      Debug("MEMORY: %s[%u]: Attempt to reallocate unallocated pointer",
            file, line);
      mp = malloc(sizeof(MemoryType));
      Assert(mp);
      mp->file = file;
      mp->line = line;
      mp->size = size;
      mp->pointer = malloc(size + sizeof(char) + 8);
      if(!mp->pointer) {
         Debug("MEMORY: %s[%u]: Failed to reallocate %d bytes.",
               file, line, (int)size);
         Assert(0);
      }
      memset(mp->pointer, 85, size);
      mp->pointer[7] = 42;
      mp->pointer[size + 8] = 42;
      mp->next = allocations;
      allocations = mp;
      return mp->pointer + 8;
   }
}

/** Release memory and log. */
void DEBUG_Release(void **ptr, const char *file, unsigned int line)
{
   MemoryType *mp, *last;
   if(!ptr) {
      Debug("MEMORY: %s[%u]: Invalid attempt to release", file, line);
   } else if(!*ptr) {
      Debug("MEMORY: %s[%u]: Attempt to delete NULL pointer",
            file, line);
   } else {
      char *cptr = (char*)*ptr - 8;
      last = NULL;
      for(mp = allocations; mp; mp = mp->next) {
         if(mp->pointer == cptr) {
            if(last) {
               last->next = mp->next;
            } else {
               allocations = mp->next;
            }

            if(cptr[mp->size + 8] != 42) {
               Debug("MEMORY: %s[%u]: The canary is dead (overflow).",
                     file, line);
            }
            if(cptr[7] != 42) {
               Debug("MEMORY: %s[%u]: The canary is dead (underflow).",
                     file, line);
            }

            memset(cptr, 0xFF, mp->size + 8 + sizeof(char));
            free(mp);
            free(cptr);
            *ptr = NULL;
            return;
         }
         last = mp;
      }
      Debug("MEMORY: %s[%u]: Attempt to delete unallocated pointer",
            file, line);
      free(*ptr);

      /* This address should cause a segfault or bus error. */
      *ptr = (void*)1;

   }
}

#undef CHECKPOINT_LIST_SIZE

#endif

