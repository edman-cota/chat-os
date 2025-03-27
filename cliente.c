#ifdef _WIN32
#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#endif

#include <json-c/json.h>

#define BUFFER_SIZE 2048

int socket_cliente;
char username[50];

void enviar_mensaje_json(const char *tipo, const char *contenido)
{
    struct json_object *mensaje_json = json_object_new_object();
    json_object_object_add(mensaje_json, "usuario", json_object_new_string(username));
    json_object_object_add(mensaje_json, "tipo", json_object_new_string(tipo));
    json_object_object_add(mensaje_json, "contenido", json_object_new_string(contenido));
    json_object_object_add(mensaje_json, "timestamp", json_object_new_int(time(NULL)));

    const char *json_str = json_object_to_json_string(mensaje_json);
    send(socket_cliente, json_str, strlen(json_str), 0);

    json_object_put(mensaje_json); // Liberar memoria
}

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

        // Parsear el JSON recibido
        struct json_object *parsed_json = json_tokener_parse(buffer);
        if (parsed_json != NULL)
        {
            struct json_object *usuario, *contenido;
            json_object_object_get_ex(parsed_json, "usuario", &usuario);
            json_object_object_get_ex(parsed_json, "contenido", &contenido);

            printf("Prueba %s: ", json_object_get_string(usuario));

            if (json_object_get_string(usuario) == NULL)
            {
                printf("%s\n",
                       json_object_get_string(contenido));
            }
            else
            {
                printf("[%s]: %s\n",
                       json_object_get_string(usuario),
                       json_object_get_string(contenido));
            }

            json_object_put(parsed_json);
        }
        else
        {
            printf("Mensaje recibido: %s\n", buffer);
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Uso: %s <Usuario> <IP Servidor> <Puerto>\n", argv[0]);
        return 1;
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("Error al inicializar Winsock.\n");
        return 1;
    }
#endif

    strncpy(username, argv[1], sizeof(username) - 1);
    char *server_ip = argv[2];
    int port = atoi(argv[3]);

    struct sockaddr_in servidor_addr;
    pthread_t hilo;

    socket_cliente = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_cliente == -1)
    {
        perror("Error al crear socket del cliente");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    servidor_addr.sin_family = AF_INET;
    servidor_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &servidor_addr.sin_addr) <= 0)
    {
        perror("Dirección IP no válida");
#ifdef _WIN32
        closesocket(socket_cliente);
        WSACleanup();
#else
        close(socket_cliente);
#endif
        return 1;
    }

    if (connect(socket_cliente, (struct sockaddr *)&servidor_addr, sizeof(servidor_addr)) == -1)
    {
        perror("Error en connect al servidor");
#ifdef _WIN32
        closesocket(socket_cliente);
        WSACleanup();
#else
        close(socket_cliente);
#endif
        return 1;
    }

    // Enviar mensaje de conexión como JSON
    enviar_mensaje_json("conexion", username);
    printf("Conectado al chat como: %s\n", username);

    pthread_create(&hilo, NULL, recibir_mensajes, NULL);

    char mensaje[BUFFER_SIZE];
    while (1)
    {
        memset(mensaje, 0, BUFFER_SIZE);
        fgets(mensaje, BUFFER_SIZE, stdin);

        // Eliminar el salto de línea del final
        mensaje[strcspn(mensaje, "\n")] = 0;

        if (strlen(mensaje) > 0)
        {
            enviar_mensaje_json("mensaje", mensaje);
        }
    }

#ifdef _WIN32
    closesocket(socket_cliente);
    WSACleanup();
#else
    close(socket_cliente);
#endif
    return 0;
}