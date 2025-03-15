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
        printf("Uso: %s <Usuario> <IP Servidor> <Puerto>\n", argv[0]);
        return 1;
    }

    char *username = argv[1];  // Nombre del usuario
    char *server_ip = argv[2]; // IP del servidor
    int port = atoi(argv[3]);  // Puerto - Usamos la función atoi (ASCII to Integer) para convertir el puerto que recibimos a un valor entero.

    struct sockaddr_in servidor_addr;
    pthread_t hilo;

    socket_cliente = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_cliente == -1)
    {
        perror("Error al crear socket del cliente");
        return 1;
    }

    servidor_addr.sin_family = AF_INET;
    servidor_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &servidor_addr.sin_addr) <= 0)
    {
        perror("Dirección IP no válida");
        return 1;
    }

    if (connect(socket_cliente, (struct sockaddr *)&servidor_addr, sizeof(servidor_addr)) == -1)
    {
        perror("Error en connect");
        return 1;
    }

    send(socket_cliente, username, strlen(username), 0);
    printf("Conectado al chat como: %s\n", username);

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