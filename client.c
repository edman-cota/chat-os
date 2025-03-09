#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

int socket_cliente;

void *recibir_mensajes(void *arg)
{
    char buffer[BUFFER_SIZE];
    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = recv(socket_cliente, buffer, BUFFER_SIZE, 0);
        if (bytes <= 0)
        {
            printf("Desconectado del servidor.\n");
            exit(1);
        }
        printf("%s\n", buffer);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Uso: %s <IP Servidor> <Puerto> <Usuario>\n", argv[0]);
        return 1;
    }

    struct sockaddr_in servidor_addr;
    pthread_t hilo;

    socket_cliente = socket(AF_INET, SOCK_STREAM, 0);
    servidor_addr.sin_family = AF_INET;
    servidor_addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &servidor_addr.sin_addr);

    if (connect(socket_cliente, (struct sockaddr *)&servidor_addr, sizeof(servidor_addr)) == -1)
    {
        perror("Error en connect");
        return 1;
    }

    send(socket_cliente, argv[3], strlen(argv[3]), 0);
    printf("Conectado al chat como: %s\n", argv[3]);

    pthread_create(&hilo, NULL, recibir_mensajes, NULL);

    char mensaje[BUFFER_SIZE];
    while (1)
    {
        memset(mensaje, 0, BUFFER_SIZE);
        fgets(mensaje, BUFFER_SIZE, stdin);
        send(socket_cliente, mensaje, strlen(mensaje), 0);
    }

    close(socket_cliente);
    return 0;
}