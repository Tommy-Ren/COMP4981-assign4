#ifndef SHARED_LIB_H
#define SHARED_LIB_H

#include "httpRequest.h"
#include <time.h>

// Global state for reload checks (optional if single `.so`)
extern time_t last_mod_time;     // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)
extern void  *current_handle;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors)

// Signature of handler used by .so files
typedef int (*RequestHandlerFunc)(int client_fd, const HTTPRequest *request);

/**
 * The entry point each .so handler must implement.
 */
int handle_request(int client_fd, const HTTPRequest *request);

/**
 * Load the shared library and resolve the handler function.
 */
RequestHandlerFunc load_request_handler(const char *so_path);

/**
 * Reload the handler if the .so file is updated.
 */
void check_for_handler_update(const char *so_path, RequestHandlerFunc *handler);

#endif    // SHARED_LIB_H
