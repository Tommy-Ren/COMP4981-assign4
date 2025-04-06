#include "../include/db.h"    // for POST storage
#include "../include/utils.h"
#include "../include/server.h"    // for GET, HEAD response helpers
#include "../include/shared_lib.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

/**************************************************************
 ******WE WILL COMPILE THIS FILE AS handler_v1.so LATER********
 **************************************************************/

/**
 * Entry point for dynamic shared library.
 * This is called by the server for each HTTP request.
 */
int handle_request(int client_fd, const HTTPRequest *request)
{
    const char *not_supported;

    if(!request || !request->method || !request->path)
    {
        const char *bad_req = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, bad_req, strlen(bad_req), 0);
        return -1;
    }

    if(strcmp(request->method, "GET") == 0)
    {
        return get_req_response(client_fd, request->path);
    }
    if(strcmp(request->method, "HEAD") == 0)
    {
        return head_req_response(client_fd, request->path);
    }
    if(strcmp(request->method, "POST") == 0)
    {
        return handle_post_request(client_fd, request, request->body);
    }

    not_supported = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
    send(client_fd, not_supported, strlen(not_supported), 0);
    return -1;
}
