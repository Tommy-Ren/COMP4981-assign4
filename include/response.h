#ifndef RESPONSE_H
#define RESPONSE_H

int head_req_response(int client_socket, const char *filePath);
int get_req_response(int client_socket, const char *filePath);
int checkIfRoot(const char *filePath, char *verified_path);

#endif
