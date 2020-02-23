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
char response_first[] = "HTTP/1.1 200 OK\r\n"
"Content-Type: text/html; charset=UTF-8\r\n\r\n"
"Request number ";
char response_second[] = " has been processed\r\n";
 
void build_response_str(char* str, int request_number) {
    // strcpy(str, response_first);
    // itoa(request_number)
    // strcat(str, response_second);

    snprintf(str, 150, "HTTP/1.1 200 OK\r\n"
                       "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                       "Request number %d has been processed",
                       request_number);
}

int main()
{
    char response_buf[150];

    int request_number = 1;

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
    
    if (bind(sock, (struct sockaddr *) &svr_addr, sizeof(svr_addr)) == -1) {
        close(sock);
        err(1, "Can't bind");
    }
    
    listen(sock, 100);
    while (1) {
        client_fd = accept(sock, (struct sockaddr *) &cli_addr, &sin_len);
        printf("got connection\n");
    
        if (client_fd == -1) {
            perror("Can't accept");
            continue;
        }

        build_response_str(response_buf, request_number);
        request_number++;    
        
        write(client_fd, response_buf, strlen(response_buf) * sizeof(char)); /*-1:'\0'*/
        
        shutdown(client_fd, SHUT_WR);
    }
}