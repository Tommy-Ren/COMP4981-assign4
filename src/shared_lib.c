#include "../include/shared_lib.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// Global state for reload checks (optional if single `.so`)
static time_t last_mod_time  = 0;
static void  *current_handle = NULL;

/**
 * Loads a shared library and returns the `handle_request` function pointer.
 * Also stores the last modification time for update tracking.
 */
RequestHandlerFunc load_request_handler(const char *so_path)
{
    struct stat        so_stat;
    void              *handle;
    RequestHandlerFunc handler;

    if(stat(so_path, &so_stat) != 0)
    {
        perror("Failed to stat shared library");
        return NULL;
    }

    last_mod_time = so_stat.st_mtime;

    handle = dlopen(so_path, RTLD_NOW);
    if(!handle)
    {
        fprintf(stderr, "dlopen error: %s\n", dlerror());
        return NULL;
    }

    // Save handle globally for reuse/reload
    current_handle = handle;

    handler = (RequestHandlerFunc)dlsym(handle, "handle_request");
    if(!handler)
    {
        fprintf(stderr, "dlsym error: %s\n", dlerror());
        dlclose(handle);
        current_handle = NULL;
        return NULL;
    }

    printf("Loaded handler from %s\n", so_path);
    return handler;
}

/**
 * Checks if the shared library at `so_path` was updated.
 * If so, unloads the current one and loads the new one into `*handler_ptr`.
 */
void check_for_handler_update(const char *so_path, RequestHandlerFunc *handler_ptr)
{
    struct stat so_stat;

    if(stat(so_path, &so_stat) != 0)
    {
        perror("Failed to stat shared library");
        return;
    }

    // Compare last modified timestamp
    if(so_stat.st_mtime > last_mod_time)
    {
        printf("Detected updated shared library. Reloading...\n");

        if(current_handle)
        {
            dlclose(current_handle);
            current_handle = NULL;
        }

        *handler_ptr = load_request_handler(so_path);
        if(!*handler_ptr)
        {
            fprintf(stderr, "Reload failed. Keeping old handler (if any).\n");
        }
    }
}
