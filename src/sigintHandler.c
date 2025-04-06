//
// Created by Kiet & Tommy on 12/1/25.
//

#include "../include/sigintHandler.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

pid_t *registered_child_pids = NULL;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)
int    registered_num_pids   = 0;       // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)

void register_child_pids(pid_t *pids, int count)
{
    registered_child_pids = pids;
    registered_num_pids   = count;
}

__attribute__((noreturn)) void sigintHandler(int sig_num)
{
    (void)sig_num;    // Suppress unused parameter warning
    printf("\nSIGINT caught. Cleaning up child processes...\n");

    // Kill all children
    for(int i = 0; i < registered_num_pids; ++i)
    {
        if(registered_child_pids[i] > 0)
        {
            printf("Killing worker PID: %d\n", registered_child_pids[i]);
            kill(registered_child_pids[i], SIGTERM);
        }
    }

    exit(0);
}
