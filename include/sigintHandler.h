//
// Created by Kiet & Tommy on 12/1/25.
//

#ifndef SIGINTHANDLER_H
#define SIGINTHANDLER_H

#include <signal.h>
#include <sys/types.h>

// External variables
extern pid_t *registered_child_pids;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)
extern int    registered_num_pids;      // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)

/**
 * Register child PIDs globally for cleanup on SIGINT.
 */
void register_child_pids(pid_t *pids, int count);

/**
 * Handle the sigint
 */
void sigintHandler(int sig_num);

#endif    // SIGINTHANDLER_H
