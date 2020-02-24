#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <err.h>
#include <string.h>
#include <pthread.h>

struct request_handler_params{
    int socket_desk;
    int request_number;
};

void build_response_str(char* str, int request_number) {
    snprintf(
        str,
        150,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n\r\n"
        "Request number %d has been processed",
        request_number
    );
}

void* request_handler(void* arg) {
    request_handler_params* params = (request_handler_params*)arg; 
    
    char response_buf[150];
    build_response_str(response_buf, params->request_number);       
    write(params->socket_desk, response_buf, strlen(response_buf) * sizeof(char)); /*-1:'\0'*/
        
    shutdown(params->socket_desk, SHUT_WR);

    delete params;
}

int main()
{
    int one = 1, client_fd;
    struct sockaddr_in svr_addr, cli_addr;
    socklen_t sin_len = sizeof(cli_addr);
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        err(1, "can't open socket");
    }
    
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
    
    int port = 8080;
    svr_addr.sin_family = AF_INET;
    svr_addr.sin_addr.s_addr = INADDR_ANY;
    svr_addr.sin_port = htons(port);
    
    if (bind(sock, (struct sockaddr*) &svr_addr, sizeof(svr_addr)) == -1) {
        close(sock);
        err(1, "Can't bind");
    }

    int request_number = 1;
    listen(sock, 10);
    
    
    while (1) {
        client_fd = accept(sock, (struct sockaddr*) &cli_addr, &sin_len);
        if (client_fd == -1) {
            perror("Can't accept");
            continue;
        }
        
        request_handler_params* params = new request_handler_params;
        params->request_number = request_number;
        params->socket_desk = client_fd;
        
        pthread_t thread;
        pthread_create(&thread, NULL, request_handler, (void*) params);

        request_number++;
    }
}