/**
 * @file command.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Handle running startup, shutdown, and restart commands.
 *
 */

#include "jwm.h"
#include "command.h"
#include "misc.h"
#include "main.h"
#include "error.h"
#include "timing.h"

#include <fcntl.h>
#include <errno.h>

/** Structure to represent a list of commands. */
typedef struct CommandNode {
   char *command;             /**< The command. */
   struct CommandNode *next;  /**< The next command in the list. */
} CommandNode;

static CommandNode *startupCommands = NULL;
static CommandNode *shutdownCommands = NULL;
static CommandNode *restartCommands = NULL;

static void RunCommands(CommandNode *commands);
static void ReleaseCommands(CommandNode **commands);
static void AddCommand(CommandNode **commands, const char *command);

/** Process startup/restart commands. */
void StartupCommands(void)
{
   if(isRestarting) {
      RunCommands(restartCommands);
   } else {
      RunCommands(startupCommands);
   }
}

/** Process shutdown commands. */
void ShutdownCommands(void)
{
   if(!shouldRestart) {
      RunCommands(shutdownCommands);
   }
}

/** Destroy the command lists. */
void DestroyCommands(void)
{
   ReleaseCommands(&startupCommands);
   ReleaseCommands(&shutdownCommands);
   ReleaseCommands(&restartCommands);
}

/** Run the commands in a command list. */
void RunCommands(CommandNode *commands) {

   CommandNode *cp;

   for(cp = commands; cp; cp = cp->next) {
      RunCommand(cp->command);
   }

}

/** Release a command list. */
void ReleaseCommands(CommandNode **commands)
{

   CommandNode *cp;

   Assert(commands);
   while(*commands) {
      cp = (*commands)->next;
      Release((*commands)->command);
      Release(*commands);
      *commands = cp;
   }

}

/** Add a command to a command list. */
void AddCommand(CommandNode **commands, const char *command)
{

   CommandNode *cp;

   Assert(commands);
   if(!command) {
      return;
   }

   cp = Allocate(sizeof(CommandNode));
   cp->next = *commands;
   *commands = cp;
   cp->command = CopyString(command);

}

/** Add a startup command. */
void AddStartupCommand(const char *command)
{
   AddCommand(&startupCommands, command);
}

/** Add a shutdown command. */
void AddShutdownCommand(const char *command) {
   AddCommand(&shutdownCommands, command);
}

/** Add a restart command. */
void AddRestartCommand(const char *command) {
   AddCommand(&restartCommands, command);
}

/** Execute an external program. */
void RunCommand(const char *command)
{

   const char *displayString;

   if(JUNLIKELY(!command)) {
      return;
   }

   displayString = DisplayString(display);
   if(!fork()) {
      close(ConnectionNumber(display));
      if(displayString && displayString[0]) {
         const size_t var_len = strlen(displayString) + 9;
         char *str = malloc(var_len);
         snprintf(str, var_len, "DISPLAY=%s", displayString);
         putenv(str);
      }
      setsid();
      execl(SHELL_NAME, SHELL_NAME, "-c", command, NULL);
      Warning(_("exec failed: (%s) %s"), SHELL_NAME, command);
      exit(EXIT_SUCCESS);
   }

}

/** Reads the output of an exernal program. */
char *ReadFromProcess(const char *command, unsigned timeout_ms)
{
   const unsigned BLOCK_SIZE = 256;
   pid_t pid;
   int fds[2];

   if(pipe(fds)) {
      Warning(_("could not create pipe"));
      return NULL;
   }
   if(fcntl(fds[0], F_SETFL, O_NONBLOCK) == -1) {
      /* We don't return here since we can still process the output
       * of the command, but the timeout won't work. */
      Warning(_("could not set O_NONBLOCK"));
   }

   pid = fork();
   if(pid == 0) {
      /* The child process. */
      if(display) {
        close(ConnectionNumber(display));
      }
      dup2(fds[1], 1);  /* stdout */
      close(fds[0]);
      close(fds[1]);
      setsid();
      execl(SHELL_NAME, SHELL_NAME, "-c", command, NULL);
      Warning(_("exec failed: (%s) %s"), SHELL_NAME, command);
      exit(EXIT_SUCCESS);
   } else if(pid > 0) {
      char *buffer;
      unsigned buffer_size, max_size;
      TimeType start_time, current_time;

      close(fds[1]);
      max_size = BLOCK_SIZE;
      buffer_size = 0;
      buffer = Allocate(max_size);

      GetCurrentTime(&start_time);
      for(;;) {
         struct timeval tv;
         unsigned long diff_ms;
         fd_set fs;
         int rc, got_read;

         FD_ZERO(&fs);
         FD_SET(fds[0], &fs);

         /* Determine the max time to sit in select. */
         GetCurrentTime(&current_time);
         diff_ms = GetTimeDifference(&start_time, &current_time);
         diff_ms = timeout_ms > diff_ms ? (timeout_ms - diff_ms) : 0;
         tv.tv_sec = diff_ms / 1000;
         tv.tv_usec = (diff_ms % 1000) * 1000;

         /* Wait for data (or a timeout). */
         do {
            rc = select(fds[0] + 1, &fs, NULL, &fs, &tv);
         } while(rc < 0 && errno == EINTR);
         if(rc == 0) {
            close(fds[0]);
            /* Timeout */
            Warning(_("timeout: %s did not complete in %u milliseconds"),
                    command, timeout_ms);
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            break;
         }

         got_read = 0;
         do {
           /* Make sure we have room to read. */
           if(buffer_size + BLOCK_SIZE > max_size) {
              max_size *= 2;
              buffer = Reallocate(buffer, max_size);
           }
           rc = read(fds[0], &buffer[buffer_size], BLOCK_SIZE);
           buffer_size += (rc > 0) ? rc : 0;
           got_read = got_read || rc > 0;
         } while(rc > 0);
         if(!got_read) {
            /* Process exited */
            close(fds[0]);
            break;
         }
      }
      close(fds[1]);
      buffer[buffer_size] = 0;
      return buffer;
   }

   return NULL;
}
