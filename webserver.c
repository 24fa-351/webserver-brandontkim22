#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

#include "http_message.h"

#define LISTEN_BACKLOG 5

typedef struct {
    int request_count;
    int total_received_bytes;
    int total_sent_bytes;
} server_stats_t;

server_stats_t server_stats = {0, 0, 0};

void respond_to_http_message(int a_client, char* status, char* content_type, char* body) {
    char response[1024];
    int body_length = body ? strlen(body) : 0;
    int response_length = snprintf(response, sizeof(response),
        "%s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "\r\n",
        status, content_type, body_length);

    // send the response to the client and update the server stats
    write(a_client, response, response_length);
    server_stats.total_sent_bytes += response_length;
    if (body) {
        write(a_client, body, body_length);
        server_stats.total_sent_bytes += body_length;
    }
}

void serve_static_file(int a_client, char* filepath) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        write(a_client, "HTTP/1.1 404 Not Found\n", 23);
        return;
    }

    // respond with a 200 OK status
    respond_to_http_message(a_client, "HTTP/1.1 200 OK",
        "image/png", NULL); // application/octet-stream??

    // send the file contents to client and update the server stats
    char buffer[1024];
    int bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        write(a_client, buffer, bytes_read);
        server_stats.total_sent_bytes += bytes_read;
    }
    close(fd);
}

// returns a properly formatted HTML doc that lists the number of requests
// received so far, and the total of received bytes and sent bytes.
void serve_stats(int a_client) {
    char stats_html[1024];
    sprintf(stats_html, "<html>\n\t<body>\n\t\t<h1>Server Stats</h1>"
                   "\n\t\t<p>Requests received: %d</p>"
                   "\n\t\t<p>Total bytes received: %d</p>"
                   "\n\t\t<p>Total bytes sent: %d</p>"
                   "\n\t</body>\n</html>\n",
                   server_stats.request_count,
                   server_stats.total_received_bytes,
                   server_stats.total_sent_bytes);
    
    // send back the stats as an HTML response
    respond_to_http_message(a_client, "HTTP/1.1 200 OK", "text/html", stats_html);
}

// returns text or HTML, summing the value of two query params in the request
// named "a" and "b" (both numeric).
void serve_calc(int a_client, http_client_message_t* http_msg) {
    char response[1024];
    int a = 0, b = 0;
    sscanf(http_msg->path, "/calc/%d/%d", &a, &b);

    int result = a + b;
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\nContent-Type: text/html\r\n\r\n"
             "<html>\n\t<body>\n\t\t<h1>Calculation Result</h1>"
             "\n\t\t<p>%d + %d = %d</p>\n\t</body>\n</html>\n",
             a, b, result);
    write(a_client, response, strlen(response));
}

void* handleConnection(void* a_client_ptr)
{
    int a_client = *(int*)a_client_ptr;
    free(a_client_ptr);

    while (1) {
        printf("Handling connection on socket_fd %d\n", a_client);
        http_client_message_t* http_msg;
        http_read_result_t result;

        server_stats.request_count++;

        read_http_message(a_client, &http_msg, &result);
        if (result == CLOSED_CONNECTION) {
            printf("Connection closed on socket_fd %d\n", a_client);
            close(a_client);
            return NULL;
        } else if (result == BAD_REQUEST) {
            printf("Bad request on socket_fd %d\n", a_client);
            close(a_client);
            return NULL;
        }

        server_stats.total_received_bytes += http_msg->msg_length;

        // parse path and respond accordingly
        if (strncmp(http_msg->path, "/static", 7) == 0) {
            char filepath[512];
            snprintf(filepath, sizeof(filepath), ".%s", http_msg->path);
            serve_static_file(a_client, filepath);
        } else if (strncmp(http_msg->path, "/stats", 6) == 0) {
            serve_stats(a_client);
        } else if (strncmp(http_msg->path, "/calc", 5) == 0) {
            serve_calc(a_client, http_msg);
        } else {
            write(a_client, "HTTP/1.1 404 PATH Not Found\n", 27);
        }

        http_client_message_free(http_msg);
    }
    printf("Connection closed on socket_fd %d\n", a_client);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    char* p;
    long port = strtol(argv[1], &p, 10);
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in socket_address;
    memset(&socket_address, '\0', sizeof(socket_address));
    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY);
    socket_address.sin_port = htons(port);

    int returnval;

    returnval = bind(
        socket_fd, (struct sockaddr*)&socket_address, sizeof(socket_address));
    if (returnval == -1) {
        perror("bind");
        return 1;
    }

    returnval = listen(socket_fd, LISTEN_BACKLOG);

    struct sockaddr_in client_address;

    while (1) {
        socklen_t client_address_len = sizeof(client_address);
        int client_fd = accept(
            socket_fd, (struct sockaddr*)&client_address, &client_address_len);
        if (client_fd == -1) {
            perror("accept");
            return 1;
        }
        int* client_fd_ptr = (int*)malloc(sizeof(int));
        *client_fd_ptr = client_fd;

        pthread_t thread;
        pthread_create(&thread, NULL, handleConnection, (void*)client_fd_ptr);
    }
   
    return 0;
}