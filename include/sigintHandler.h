//
// Created by Kiet & Tommy on 12/1/25.
//

#ifndef SIGINTHANDLER_H
#define SIGINTHANDLER_H

#include <signal.h>

/**
 * Registers an array of child PIDs to be tracked by the SIGINT handler.
 * Must be called before the server starts forking.
 * @param pids Pointer to the array of child PIDs.
 * @param count Number of worker processes.
 */
void register_child_pids(pid_t *pids, int count);

/**
 * Catches SIGINT and handles cleanup.
 * @param sig_num The signal number.
 */
void sigintHandler(int sig_num);

#endif    // SIGINTHANDLER_H
