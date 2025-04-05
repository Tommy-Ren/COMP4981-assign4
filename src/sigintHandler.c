//
// Created by Kiet & Tommy on 12/1/25.
//

#include "../include/sigintHandler.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t *child_pids   = NULL;
static int    num_children = 0;

void register_child_pids(pid_t *pids, int count)
{
    child_pids   = pids;
    num_children = count;
}

void sigintHandler(int sig_num)
{
    // Reset signal handler
    signal(SIGINT, sigintHandler);

    printf("\nSIGINT received (signal %d), shutting down server gracefully...\n", sig_num);

    // Terminate all child processes
    if(child_pids)
    {
        for(int i = 0; i < num_children; i++)
        {
            if(child_pids[i] > 0)
            {
                kill(child_pids[i], SIGTERM);
            }
        }

        // Wait for children to terminate
        for(int i = 0; i < num_children; i++)
        {
            waitpid(child_pids[i], NULL, 0);
        }
    }

    printf("All child processes terminated. Server exiting.\n");

    exit(EXIT_SUCCESS);
}
