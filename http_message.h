#ifndef HTTP_MESSAGE_H
#define HTTP_MESSAGE_H

typedef struct msg {
    char* method;
    char* path;
    char* protocol;
    char* headers;
    char* body;
    int body_length;
    int msg_length;
} http_client_message_t;

typedef enum {
    BAD_REQUEST,
    CLOSED_CONNECTION,
    MESSAGE
} http_read_result_t;

// responses: a message, bad request, or closed connection
// allocates and returns a message if the message is valid
void read_http_message(int a_client,
    http_client_message_t** msg,
    http_read_result_t* result);

void http_client_message_free(http_client_message_t* http_msg);

#endif