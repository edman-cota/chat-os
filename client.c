#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

int client_socket;
char username[50];

void *receive_messages(void *arg);

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Uso: %s <nombre_usuario> <IP_servidor> <puerto>\n", argv[0]);
        return -1;
    }

    strcpy(username, argv[1]);

    struct sockaddr_in server_addr;
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1)
    {
        perror("Error al crear el socket");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[3]));
    inet_pton(AF_INET, argv[2], &server_addr.sin_addr);

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error en connect");
        return -1;
    }

    send(client_socket, username, strlen(username), 0);

    pthread_t tid;
    pthread_create(&tid, NULL, receive_messages, NULL);

    char buffer[BUFFER_SIZE];
    while (1)
    {
        fgets(buffer, BUFFER_SIZE, stdin);
        send(client_socket, buffer, strlen(buffer), 0);
    }

    close(client_socket);
    return 0;
}

void *receive_messages(void *arg)
{
    char buffer[BUFFER_SIZE];
    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0)
        {
            printf("Desconectado del servidor\n");
            exit(0);
        }
        printf("%s\n", buffer);
    }
}