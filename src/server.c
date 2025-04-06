//
// Created by Kiet & Tommy on 12/1/25.
//

#include "../include/server.h"
#include "../include/db.h"
#include "../include/fileTools.h"
#include "../include/shared_lib.h"
#include "../include/sigintHandler.h"
#include "../include/stringTools.h"
#include "../include/utils.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// static int set_nonblocking(int sockfd)
// {
//     int flags = fcntl(sockfd, F_GETFL, 0);
//     if(flags == -1)
//     {
//         perror("fcntl F_GETFL");
//         return -1;
//     }
//     if(fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
//     {
//         perror("fcntl F_SETFL O_NONBLOCK");
//         return -1;
//     }
//     return 0;
// }

/**
 * Function to load the request handler from the shared library
 * @param so_path path to the shared library
 * @return function pointer to the request handler
 */
int start_prefork_server(const char *ip, const char *port, const char *so_path, int num_workers)
{
    struct serverInformation server;
    pid_t                   *child_pids;
    RequestHandlerFunc       handler;

    server.ip   = strdup(ip);
    server.port = strdup(port);
    child_pids  = NULL;
    handler     = NULL;

    // Setup socket
    server.fd = socket_create();
    if(socket_bind(server) < 0)
    {
        perror("Socket bind failed");
        goto cleanup;
    }

    start_listen(server.fd);

    // Load shared library handler
    handler = load_request_handler(so_path);
    if(!handler)
    {
        fprintf(stderr, "Failed to load handler from .so\n");
        goto cleanup;
    }

    // Fork worker processes
    child_pids = malloc((size_t)num_workers * sizeof(pid_t));
    if(!child_pids)
    {
        perror("malloc failed");
        goto cleanup;
    }

    register_child_pids(child_pids, num_workers);

    for(int i = 0; i < num_workers; i++)
    {
        pid_t pid = fork();
        if(pid < 0)
        {
            perror("fork failed");
            goto cleanup;
        }

        if(pid == 0)
        {
            // === CHILD PROCESS ===
            printf("[Worker %d] Started with PID %d\n", i, getpid());

            while(1)
            {
                TokenAndStr  firstLine;
                HTTPRequest *request;
                int          client_fd;
                char         buffer[BUFFER_SIZE];
                ssize_t      bytes;

                client_fd = accept(server.fd, NULL, NULL);
                if(client_fd < 0)
                {
                    perror("accept failed");
                    continue;
                }
                printf("[Worker %d] Connection accepted\n", getpid());

                bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
                if(bytes <= 0)
                {
                    perror("recv failed");
                    close(client_fd);
                    continue;
                }
                buffer[bytes] = '\0';
                printf("Received: %s", buffer);

                // Get the first line (request line)
                firstLine = getFirstToken(buffer, "\n");
                request   = initializeHTTPRequestFromString(firstLine.token);
                free(firstLine.originalStr);

                // Handle POST body (if any)
                if(strcmp(request->method, "POST") == 0)
                {
                    // Detect start of body: after empty line (\r\n\r\n)
                    char *body_start = strstr(buffer, "\r\n\r\n");
                    if(body_start)
                    {
                        body_start += 4;    // Skip past \r\n\r\n
                        setHTTPRequestBody(request, body_start);
                    }
                }

                // Handle request
                check_for_handler_update(so_path, &handler);
                handler(client_fd, request);

                // Clean up
                close(client_fd);
                free(request->method);
                free(request->path);
                free(request->protocol);
                if(request->body)
                {
                    free(request->body);
                }
                free(request);
            }
        }
        else
        {
            // === PARENT PROCESS ===
            child_pids[i] = pid;
        }
    }

    printf("[Parent] Monitoring worker processes...\n");

    // Monitor & restart crashed workers
    while(1)
    {
        sleep(1);
        for(int i = 0; i < num_workers; i++)
        {
            pid_t exited = waitpid(child_pids[i], NULL, WNOHANG);
            if(exited == 0)
            {
                // still alive
                continue;
            }

            if(exited > 0)
            {
                pid_t new_pid;

                // Worker crashed, restart
                new_pid = fork();
                printf("[Parent] Worker %d (PID %d) died. Restarting...\n", i, exited);

                if(new_pid == 0)
                {
                    printf("[Worker %d] Restarted with PID %d\n", i, getpid());

                    while(1)
                    {
                        int          client_fd = accept(server.fd, NULL, NULL);
                        char         buffer[BUFFER_SIZE];
                        ssize_t      bytes;
                        TokenAndStr  firstLine;
                        HTTPRequest *request;

                        if(client_fd < 0)
                        {
                            perror("accept failed");
                            continue;
                        }
                        printf("[Worker %d] Connection accepted\n", getpid());

                        bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
                        if(bytes <= 0)
                        {
                            close(client_fd);
                            continue;
                        }

                        buffer[bytes] = '\0';

                        firstLine = getFirstToken(buffer, "\n");
                        request   = initializeHTTPRequestFromString(firstLine.token);
                        free(firstLine.originalStr);

                        if(strcmp(request->method, "POST") == 0)
                        {
                            char *body_start = strstr(buffer, "\r\n\r\n");
                            if(body_start)
                            {
                                body_start += 4;
                                setHTTPRequestBody(request, body_start);
                            }
                        }

                        check_for_handler_update(so_path, &handler);
                        handler(client_fd, request);

                        close(client_fd);
                        free(request->method);
                        free(request->path);
                        free(request->protocol);
                        if(request->body)
                        {
                            free(request->body);
                        }
                        free(request);
                    }
                }
                child_pids[i] = new_pid;
            }
        }
    }

cleanup:
    if(child_pids)
    {
        free(child_pids);
    }
    if(server.port)
    {
        free(server.port);
    }
    if(server.ip)
    {
        free(server.ip);
    }
    server_close(server);
    return 0;
}

/**
 * Function to handle the server setup
 * @param passedServerInfo array of server information
 * @return 0 if success
 */
int server_setup(char *passedServerInfo[])
{
    const char *ip      = passedServerInfo[0];
    const char *port    = passedServerInfo[1];
    const char *so_path = "../handlers/handler_v1.so";
    const int   workers = 4;    // Number of worker processes

    return start_prefork_server(ip, port, so_path, workers);
}

/**
 * Function to create a socket
 * @return server socket fd
 */
int socket_create(void)
{
    int serverSocket;
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket == -1)
    {
        perror("Socket create failed");
        exit(EXIT_FAILURE);
    }
    return serverSocket;
}

/**
 * Function to bind the socket to the given IP and port
 * @param activeServer server information
 * @return 0 if success
 */
int socket_bind(struct serverInformation activeServer)
{
    struct sockaddr_in server_address;
    int                port;
    char              *endptr;
    const int          decimalBase = 10;

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;

    if(inet_pton(AF_INET, activeServer.ip, &server_address.sin_addr) <= 0)
    {
        perror("Invalid IP address");
        return -1;
    }

    port = (int)strtol(activeServer.port, &endptr, decimalBase);

    if(*endptr != '\0' && *endptr != '\n')
    {
        perror("Invalid port number");
        return -1;
    }

    server_address.sin_port = htons((uint16_t)port);

    if(bind(activeServer.fd, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Socket bound successfully to %s:%s.\n", activeServer.ip, activeServer.port);

    return 0;
}

void start_listen(int server_fd)
{
    if(listen(server_fd, SOMAXCONN) == -1)
    {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Listening for incoming connections...\n");
}

/**
 * Function to close the given server socket
 * @param activeServer server socket fd
 * @return 0 if success
 */
int server_close(struct serverInformation activeServer)
{
    if(close(activeServer.fd) == -1)
    {
        perror("close failed");
        exit(EXIT_FAILURE);
    }
    printf("Closing the server.\n");
    return 0;
}

/**
 * Function to close a client socket
 * @param activeClient client socket fd
 * @return 0 if success
 */
int client_close(int activeClient)
{
    if(close(activeClient) == -1)
    {
        perror("close failed");
        exit(EXIT_FAILURE);
    }
    printf("Closing the client.\n");
    return 0;
}
