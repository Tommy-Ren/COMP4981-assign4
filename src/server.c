//
// Created by Kiet & Tommy on 12/1/25.
//

#include "../include/server.h"
#include "../include/db.h"
#include "../include/fileTools.h"
#include "../include/shared_lib.h"
#include "../include/stringTools.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <ndbm.h>
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

#define BUFFER_SIZE 4096

// #define STATUS_INTERNAL_SERVER_ERR 500
// #define STATUS_OK 200
// #define STATUS_RES_CREATED 201

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
 * Function to load the request handler from the shared library
 * @param so_path path to the shared library
 * @return function pointer to the request handler
 */
int start_prefork_server(const char *ip, const char *port, const char *so_path, int num_workers)
{
    struct serverInformation server;
    pid_t                   *child_pids;
    RequestHandlerFunc       handler;

    server.ip   = (char *)ip;
    server.port = (char *)port;

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
    child_pids = malloc(num_workers * sizeof(pid_t));
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
                int client_fd = accept(server.fd, NULL, NULL);
                if(client_fd < 0)
                {
                    perror("accept failed");
                    continue;
                }

                char    buffer[BUFFER_SIZE];
                ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
                if(bytes <= 0)
                {
                    close(client_fd);
                    continue;
                }

                buffer[bytes] = '\0';

                // Get the first line (request line)
                TokenAndStr  firstLine = getFirstToken(buffer, "\n");
                HTTPRequest *request   = initializeHTTPRequestFromString(firstLine.token);
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
                    free(request->body);
                free(request);
            }

            exit(EXIT_SUCCESS);    // never reached unless loop breaks
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
                continue;    // still alive

            if(exited > 0)
            {
                // Worker crashed, restart
                printf("[Parent] Worker %d (PID %d) died. Restarting...\n", i, exited);
                pid_t new_pid = fork();
                if(new_pid == 0)
                {
                    printf("[Worker %d] Restarted with PID %d\n", i, getpid());

                    while(1)
                    {
                        int client_fd = accept(server.fd, NULL, NULL);
                        if(client_fd < 0)
                        {
                            perror("accept failed");
                            continue;
                        }

                        char    buffer[BUFFER_SIZE];
                        ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
                        if(bytes <= 0)
                        {
                            close(client_fd);
                            continue;
                        }

                        buffer[bytes] = '\0';

                        TokenAndStr  firstLine = getFirstToken(buffer, "\n");
                        HTTPRequest *request   = initializeHTTPRequestFromString(firstLine.token);
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
                            free(request->body);
                        free(request);
                    }
                    exit(EXIT_SUCCESS);
                }

                child_pids[i] = new_pid;
            }
        }
    }

cleanup:
    if(child_pids)
        free(child_pids);
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

    printf("Starting pre-forked server on %s:%s with %d workers...\n", ip, port, workers);

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

/**
 * Function to check if a filePath is the root. If it is the root, then it will
 * change the verified_path to a default value
 * @param filePath path of the resource
 * @param verified_path the file path of either ./html/index.html or a valid
 * request
 * @return 0 if default, 1 if request
 */
// todo move to server.h, remove static
static int checkIfRoot(const char *filePath, char *verified_path)
{
    if(strcmp(filePath, "../data/") == 0)
    {
        printf("Requesting default file path...");
        strcpy(verified_path, "index.html");
    }
    else
    {
        strcpy(verified_path, filePath);
        return 1;
    }
    return 0;
}

/**
 * Function to construct the response and send to client socket from a given
 * resource Only called when a resource is confirmed to exist
 * @param client_socket client to send the response to
 * @param content content of the resource requested
 * @return 0 if success
 */
// todo add status codes and handle each situation based on that
int send_response_resource(int client_socket, const char *content, size_t content_length)
{
    char   response[BUFFER_SIZE];
    size_t total_sent = 0;
    snprintf(response, BUFFER_SIZE, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", content_length);

    // send the response header
    if(send(client_socket, response, strlen(response), 0) == -1)
    {
        perror("Error sending response header");
        return -1;
    }

    // send content
    while(total_sent < content_length)
    {
        ssize_t sent = send(client_socket, content + total_sent, content_length - total_sent, 0);
        if(sent == -1)
        {
            perror("Error sending content");
            return -1;
        }
        total_sent += (size_t)sent;
    }
    return 0;
}

/**
 * Function to construct and send the head request response
 * @param client_socket the client that will get the response
 * @param content_length the length of the content in the requested resource
 * @return 0 if success
 */ // todo <-- additional status codes
int send_response_head(int client_socket, size_t content_length)
{
    char response[BUFFER_SIZE];
    snprintf(response, BUFFER_SIZE, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", content_length);

    // Send the response header
    if(send(client_socket, response, strlen(response), 0) == -1)
    {
        perror("Error sending response header");
        return -1;
    }

    return 0;
}

/**
 * Function to read the resource, save the resource into a malloc, send the
 * resource back to the client
 * @param client_socket client socket that sends the req
 * @return 0 if success
 */
// todo break down into smaller functions
int get_req_response(int client_socket, const char *filePath)
{
    char  *filePathWithDot;
    FILE  *resource_file;
    long   totalBytesRead;
    char  *file_content;
    char   verified_path[BUFFER_SIZE];
    size_t bytesRead;

    // todo shift all get/head functions into a single function to port to both
    // check if filePath is root
    checkIfRoot(filePath, verified_path);

    // append "./" to filePathWithDot
    filePathWithDot = addCharacterToStart(verified_path, "../data");
    if(filePathWithDot == NULL)
    {
        perror(". character not added");
        return -1;
    }

    // open file
    resource_file = fopen(filePathWithDot, "rbe");
    if(resource_file == NULL)
    {
        fprintf(stderr, "Error opening resource file: %s\n", filePathWithDot);
        free(filePathWithDot);
        return -1;
    }
    free(filePathWithDot);

    // move cursor to the end of the file, read the position in bytes, reset
    // cursor
    fseek(resource_file, 0, SEEK_END);
    totalBytesRead = ftell(resource_file);
    fseek(resource_file, 0, SEEK_SET);

    // allocate memory
    file_content = (char *)malloc((unsigned long)(totalBytesRead + 1));
    if(file_content == NULL)
    {
        perror("Error allocating memory");
        fclose(resource_file);
        return -1;
    }

    // read into buffer
    bytesRead = fread(file_content, 1, (unsigned long)totalBytesRead, resource_file);
    if((long)bytesRead != totalBytesRead)
    {
        perror("Error reading HTML file");
        free(file_content);
        fclose(resource_file);
        return -1;
    }
    fclose(resource_file);

    // create response and send
    if(send_response_resource(client_socket, file_content, bytesRead) == -1)
    {
        free(file_content);
        return -1;
    }

    // free resources on success
    free(file_content);
    return 0;
}

/**
 * Function to handle a HEAD request and send back the header
 * @return 0 if success
 */
int head_req_response(int client_socket, const char *filePath)
{
    /*
     * Steps:
     * Check if root
     * filePathWithDot
     * open file (check if exists)
     * close file
     * send response based on this
     */
    char *filePathWithDot;
    FILE *resource_file;
    char  verified_path[BUFFER_SIZE];
    long  totalBytesRead;

    // check if filePath is root
    checkIfRoot(filePath, verified_path);

    // append "." to filePathWithDot
    filePathWithDot = addCharacterToStart(verified_path, "../data");
    if(filePathWithDot == NULL)
    {
        perror(". character not added");
        return -1;
    }

    // open file
    resource_file = fopen(filePathWithDot, "rbe");
    free(filePathWithDot);
    if(resource_file == NULL)
    {
        perror("Error opening resource file");
        return -1;
    }

    fseek(resource_file, 0, SEEK_END);
    totalBytesRead = ftell(resource_file);
    fseek(resource_file, 0, SEEK_SET);

    // send header
    send_response_head(client_socket, (size_t)totalBytesRead);

    // close file
    fclose(resource_file);

    return 0;
}

/**
 * POST handling helper — stores POST body into ndbm.
 */
int handle_post_request(int client_socket, const HTTPRequest *request, const char *body)
{
    if(!request || !body || strlen(body) == 0)
    {
        const char *bad_request = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client_socket, bad_request, strlen(bad_request), 0);
        return -1;
    }

    DBO dbo;
    dbo.name = "../db/post_data.db";    // Path to your ndbm database

    if(store_post_entry(&dbo, body, "entry_id") != 0)
    {
        const char *internal_error = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
        send(client_socket, internal_error, strlen(internal_error), 0);
        return -1;
    }

    const char *created_response = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n";
    send(client_socket, created_response, strlen(created_response), 0);
    return 0;
}
