//
// Created by Kiet & Tommy on 12/1/25.
//

#ifndef SIGINTHANDLER_H
#define SIGINTHANDLER_H

#include <signal.h>
#include <sys/types.h>

void sigintHandler(int sig_num);

/**
 * Register child PIDs globally for cleanup on SIGINT.
 */
void register_child_pids(pid_t *pids, int count);

#endif    // SIGINTHANDLER_H
