#include "../include/utils.h"
#include "../include/db.h"
#include "../include/server.h"
#include "../include/stringTools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

/**
 * Function to check if a filePath is the root. If it is the root, then it will
 * change the verified_path to a default value
 * @param filePath path of the resource
 * @param verified_path the file path of either ./html/index.html or a valid
 * request
 * @return 0 if default, 1 if request
 */
// todo move to server.h, remove static
int checkIfRoot(const char *filePath, char *verified_path)
{
    if(strcmp(filePath, "../data/") == 0)
    {
        printf("Requesting default file path...");
        strncpy(verified_path, "index.html", BUFFER_SIZE - 1);
        verified_path[BUFFER_SIZE - 1] = '\0';
    }
    else
    {
        strncpy(verified_path, filePath, BUFFER_SIZE - 1);
        verified_path[BUFFER_SIZE - 1] = '\0';
        return 1;
    }
    return 0;
}

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

    printf("head_req_response\n");

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

    printf("get_req_response\n");

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

    // Check if any data was actually read
    if(totalBytesRead > 0)
    {
        // Allocate memory for the file content
        file_content = (char *)malloc((unsigned long)(totalBytesRead + 1));
        if(file_content == NULL)
        {
            perror("Error allocating memory");
            fclose(resource_file);
            return -1;
        }
        // Read the file content into memory (assuming fread is used)
        bytesRead = fread(file_content, 1, (unsigned long)totalBytesRead, resource_file);
        if(bytesRead != (unsigned long)totalBytesRead)
        {
            perror("Error reading file");
            free(file_content);    // Don't forget to free memory if reading fails
            fclose(resource_file);
            return -1;
        }

        file_content[totalBytesRead] = '\0';    // Null-terminate the string
    }
    else
    {
        // Handle the case where no data was read
        file_content = NULL;    // No data to store
        // Optionally, handle the error if no data is read
        fprintf(stderr, "No data was read from the file\n");
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
 * POST handling helper — stores POST body into ndbm.
 */
int handle_post_request(int client_socket, const HTTPRequest *request, const char *body)
{
    DBO   dbo;
    char *created_response = NULL;

    printf("handle_post_request\n");

    if(!request || !body || strlen(body) == 0)
    {
        const char *bad_request = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client_socket, bad_request, strlen(bad_request), 0);
        return -1;
    }

    dbo.name         = strdup("../db/post_data.db");    // Path to your ndbm database
    created_response = strdup("HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n");

    if(store_post_entry(&dbo, body, "entry_id") != 0)
    {
        const char *internal_error = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
        send(client_socket, internal_error, strlen(internal_error), 0);
        free((void *)created_response);    // ✅ safe even if it's NULL
        return -1;
    }

    // ✅ Only send and then free after use
    if(created_response != NULL)
    {
        send(client_socket, created_response, strlen(created_response), 0);
        free((void *)created_response);
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
