//
// Created by Kiet & Tommy on 12/1/25.
//

#include "../include/client.h"
#include "../include/server.h"
#include "../include/sigintHandler.h"
#include "../include/stringTools.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define USAGE "Usage: -t type -i ip -p port\n"

// Struct to hold command-line args
struct arguments
{
    char *type;
    char *ip;
    char *port;
};

// Parse arguments
static struct arguments parse_args(int argc, char *argv[]);
// Handle logic based on -t type
static int handle_args(struct arguments args);

// drives code
int main(int argc, char *argv[])
{
    setup_sigint_handler();

    // Parse CLI args and dispatch
    return handle_args(parse_args(argc, argv));
}

static struct arguments parse_args(int argc, char *argv[])
{
    int              opt;
    struct arguments args;

    // Initialize struct
    args.type = NULL;
    args.ip   = NULL;
    args.port = NULL;

    // Parse arguments
    while((opt = getopt(argc, argv, "t:i:p:")) != -1)
    {
        switch(opt)
        {
            case 't':
                args.type = optarg;
                break;
            case 'i':
                args.ip = optarg;
                break;
            case 'p':
                args.port = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s -t type -i ip -p port\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if(!args.type || !args.ip || !args.port || !checkIfCharInString(args.ip, '.') || checkIfCharInString(args.port, '.'))
    {
        fprintf(stderr, USAGE);
        exit(EXIT_FAILURE);
    }

    return args;
}

static int handle_args(struct arguments args)
{
    char *serverInformation[2];
    serverInformation[0] = args.ip;
    serverInformation[1] = args.port;

    printf("Running on %s:%s\n", args.ip, args.port);

    if(args.type == NULL || args.ip == NULL || args.port == NULL)
    {
        return 1;
    }

    if(strcmp(args.type, "client") == 0)
    {
        // client
        return connect_client(serverInformation);
    }

    if(strcmp(args.type, "server") == 0)
    {
        const char *so_path     = "./handlers/handler_v1.so";
        int         num_workers = 4;

        printf("Starting pre-fork server on %s:%s with %d workers using %s\n", args.ip, args.port, num_workers, so_path);

        return start_prefork_server(args.ip, args.port, so_path, num_workers);
    }

    // Handle invalid type case
    fprintf(stderr,
            "Error: Invalid type: %s\n"
            "Available: 'client', 'server'\n%s",
            args.type,
            USAGE);
    return 1;
}
