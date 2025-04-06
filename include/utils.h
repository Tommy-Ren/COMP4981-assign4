#ifndef RESPONSE_H
#define RESPONSE_H

#include "../include/httpRequest.h"

int head_req_response(int client_socket, const char *filePath);
int get_req_response(int client_socket, const char *filePath);
int checkIfRoot(const char *filePath, char *verified_path);
int handle_post_request(int client_socket, const HTTPRequest *request, const char *body);

#endif
