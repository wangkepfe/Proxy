#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <ctype.h>

#include "proxy.h"
#include "util.h"
#include "proxy_clientside.h"
#include "midlayer.h"

/* sigChldHandler
 *
 * Handler for incoming signals
 */
void sigChldHandler(int s)
{
        // waitpid() might overwrite errno, so we save and restore it:
        int saved_errno = errno;
        while(waitpid(-1, NULL, WNOHANG) > 0);
        errno = saved_errno;
}

/* initSigHandlers
 *
 * Set up handler functions for signals sent to the process
 * Reap dead processes/threads left behind
 */
int initSigHandlers(void)
{
        struct sigaction sa;

        sa.sa_handler = sigChldHandler; // reap all dead processes
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        if (sigaction(SIGCHLD, &sa, NULL) == -1)
                {
                        perror("sigaction");
                        return -1;
                }

        return 0;
}

/* main
 *
 * Entry point for the proxy
 *
 * @param argc Number of commandline parameters
 * @param argv Array containing commandline parameters
 */
int main(int argc, char *argv[])
{
        if(argc >= 2)
        {

                for(size_t i = 0; i < strlen(argv[1]); ++i)
                {
                        if(!isdigit(argv[1][i]))
                        {
                                printf("ERROR: Provided port may only contain digits\n");
                                return -1;
                        }
                }

                initSigHandlers();
                startProxy(argv[1]);
        }
        else
        {
                printf("Usage: %s <port>\n", argv[0]);
        }
}
