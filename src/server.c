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
#include <errno.h>
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

static int set_nonblocking(int sockfd)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    if(flags == -1)
    {
        perror("fcntl F_GETFL");
        return -1;
    }
    if(fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl F_SETFL O_NONBLOCK");
        return -1;
    }
    return 0;
}

/**
 * Function to handle the recvmsg from the client
 * @param server_fd server.fd
 * @param so_path path to the shared library
 */
static void handle_recvmsg(int server_fd, const char *so_path, RequestHandlerFunc handler)
{
    struct pollfd pfd;
    int           ret;

    // Set up the pollfd structure to monitor the server socket for readability
    pfd.fd     = server_fd;
    pfd.events = POLLIN;

    // Wait for incoming connections
    ret = poll(&pfd, 1, -1);
    if(ret < 0)
    {
        perror("poll failed");
        return;
    }

    if(ret == 0)
    {
        printf("poll timed out\n");
        return;
    }

    if(pfd.revents & POLLIN)
    {
        struct pollfd client_pfd;
        int           client_ret;
        int           client_fd = accept(server_fd, NULL, NULL);
        if(client_fd < 0)
        {
            perror("accept failed");
            return;
        }
        printf("\n[Worker %d] Connection accepted the client fd %d\n", getpid(), client_fd);

        // Set socket to non-blocking mode
        if(set_nonblocking(client_fd) < 0)
        {
            close(client_fd);
            return;
        }

        // Now that the client is connected, handle the client request using poll for reading
        client_pfd.fd     = client_fd;
        client_pfd.events = POLLIN;

        // Wait for data from the client
        client_ret = poll(&client_pfd, 1, -1);    // Timeout is set to -1 (wait indefinitely)
        if(client_ret < 0)
        {
            perror("poll failed");
            close(client_fd);
            return;
        }

        if(client_ret == 0)
        {
            printf("poll timed out for client fd %d\n", client_fd);
            close(client_fd);
            return;
        }

        if(client_pfd.revents & POLLIN)
        {
            ssize_t      bytes;
            TokenAndStr  firstLine;
            HTTPRequest *request;
            char         buffer[BUFFER_SIZE];

            struct msghdr msg = {0};
            struct iovec  iov;
            iov.iov_base = buffer;
            iov.iov_len  = sizeof(buffer);

            msg.msg_name       = NULL;    // No address information
            msg.msg_namelen    = 0;
            msg.msg_iov        = &iov;    // Pointer to the vector of buffers
            msg.msg_iovlen     = 1;       // Number of buffers
            msg.msg_control    = NULL;    // No control messages
            msg.msg_controllen = 0;       // Length of control data
            msg.msg_flags      = 0;       // Flags (not using any here)

            bytes = recvmsg(client_fd, &msg, 0);
            if(bytes < 0)
            {
                perror("recvmsg failed");
                close(client_fd);
                return;
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
            free(request->method);
            free(request->path);
            free(request->protocol);
            if(request->body)
            {
                free(request->body);
            }
            free(request);
        }
        close(client_fd);
    }
}

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
                handle_recvmsg(server.fd, so_path, handler);
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
                        handle_recvmsg(server.fd, so_path, handler);
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
    const char *so_path = "../data/handler/handler_v1.so";
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
