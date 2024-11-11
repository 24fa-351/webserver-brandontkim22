#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>

#include "http_message.h"

bool is_complete_http_message(char* buffer) {
    if (strlen(buffer) < 10) {
        return false;
    }
    if (strncmp(buffer, "GET ", 4) != 0) {
        return false;
    }
    if (strncmp(buffer + strlen(buffer) - 4, "\r\n\r\n", 4) != 0) {
        return false;
    }
    return true;
}

void read_http_message(
    int a_client, http_client_message_t** msg, http_read_result_t* result)
    {
        *msg = malloc(sizeof(http_client_message_t));
        char buffer[1024];
        strcpy(buffer, "");

        while ( !is_complete_http_message(buffer) ) {
            int bytes_read = read(a_client, buffer + strlen(buffer),
                sizeof(buffer) - strlen(buffer));
            if (bytes_read <= 0) {
                *result = CLOSED_CONNECTION;
                return;
            }
            if (bytes_read < 0) {
                *result = BAD_REQUEST;
                return;
            }
        }

        // save the message length for server stats
        (*msg)->msg_length = strlen(buffer);

        // parse and save the given method, path, and protocol if they exist
        char method[16], path[1024], protocol[16];
        sscanf(buffer, "%s %s %s", method, path, protocol);
        if (strcmp(method, "GET") != 0) {
            printf("Received from socket_fd %d: %s\n", a_client, buffer);
            write(a_client, "HTTP/1.1 405 Method Not Allowed\n", 31);
        }
        if (strcmp(protocol, "HTTP/1.1") != 0) {
            printf("Received from socket_fd %d: %s\n", a_client, buffer);
            write(a_client, "HTTP/1.1 505 HTTP Version Not Supported\n", 40);
            *result = BAD_REQUEST;
            return;
        }
        (*msg)->method = strdup(method);
        (*msg)->path = strdup(path);
        (*msg)->protocol = strdup(protocol);
        
        printf("%s %s %s\n", (*msg)->method, (*msg)->path, (*msg)->protocol);
        *result = MESSAGE;
    }

void http_client_message_free(http_client_message_t* http_msg) { free(http_msg); }