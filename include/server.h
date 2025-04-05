//
// Created by Kiet & Tommy on 12/1/25.
//

#ifndef MAIN_SERVER_H
#define MAIN_SERVER_H

#include "httpRequest.h"    // Not shared_lib.h — only server-side logic
#include <signal.h>

// struct to hold the info for server
struct serverInformation
{
    int   fd;
    char *ip;
    char *port;
};

// struct to hold the info for client
struct clientInformation
{
    int fd;
};

// Pre-fork entry point
int start_prefork_server(const char *ip, const char *port, const char *so_path, int num_workers);

// Legacy file functions (still useful for GET/HEAD)
int  server_setup(char *passedServerInfo[]);
int  socket_create(void);
int  socket_bind(struct serverInformation server);
void start_listen(int server_fd);
int  server_close(struct serverInformation server);
int  client_close(int client);

// Static HTTP helpers (used by handler_v1.so)
int send_response_resource(int client_socket, const char *content, size_t content_length);
int send_response_head(int client_socket, size_t content_length);
int get_req_response(int client_socket, const char *filePath);
int head_req_response(int client_socket, const char *filePath);
int handle_post_request(int client_socket, const HTTPRequest *request, const char *body);

#endif    // MAIN_SERVER_H
