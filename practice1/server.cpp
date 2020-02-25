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
#include <iostream>
#include <sstream>
#include <fstream>

using namespace std;

// Размер символьного буфера для ответа клиенту
const unsigned int BUF_LENGTH = 175;

struct request_handler_params{
    // Дескриптор сокета клиента
    int socket_desk;
    // Порядковый номер запроса клиента
    int request_number;
};

// Функция для формирования строки с ответом для клиента
void build_response_str(char* str, int request_number) {
    // Добавляем номер запроса в строку с ответом
    snprintf(
        str,
        BUF_LENGTH,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n\r\n"
        "Request number %d has been processed ",
        request_number
    );

    // Записываем вывод php-скрипта в промежуточный файл
    system("php ./version.php > tmp.txt");
    
    ifstream file("tmp.txt", ios::in | ios::binary | ios::ate);
    if (file.is_open()) {
        streampos size = file.tellg();
        char* memblock = new char[size];
        
        // Считываем вывод php-скрипта из промежуточного файла
        file.seekg(0, ios::beg);
        file.read(memblock, size);
        file.close();
        
        // Добавляем версию php, полученную из скрипта в строку с ответом
        strcat(str, memblock);

        delete[] memblock;
    } else {
        cout << "unable to open tmp file" << endl;
    }
}

// Функция-обработчик запроса
void* request_handler(void* arg) {
    request_handler_params* params = (request_handler_params*)arg; 
    
    // Формируем строку с ответом
    char response_buf[BUF_LENGTH];
    build_response_str(response_buf, params->request_number);       
    
    // Отправляем строку пользователю
    write(params->socket_desk, response_buf, strlen(response_buf) * sizeof(char)); /*-1:'\0'*/

    shutdown(params->socket_desk, SHUT_WR);

    delete params;

}

int main()
{
    int res;
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

    listen(sock, 5);
    
    // Проинициализируем переменную для хранения атрибутов потока
	pthread_attr_t thread_attr;
	res = pthread_attr_init(&thread_attr);
	if (res != 0)
	{
		cout << "Cannot create thread attribute: " << strerror(res) << endl;
		exit(-1);
	}

    size_t stack_size = 1024 * 1024;
	// Поменяем размер стека
	res = pthread_attr_setstacksize(&thread_attr, stack_size);
	if (res != 0)
	{
		cout << "Setting stack size attribute failed: " << strerror(res)
			<< endl;
		exit(-1);
	}

    // Порядковый номер запроса клиента
    int request_number = 1;
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
        res = pthread_create(&thread, &thread_attr, request_handler, (void*) params);
        if (res != 0)
        {
            cout << "Cannot create a thread: " << strerror(res) << endl;
            exit(-1);
        }

        request_number++;
    }
}