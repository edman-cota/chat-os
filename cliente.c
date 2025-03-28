#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>
#include <json-c/json.h>

#define MAX_MESSAGES 100
#define CLEAR_SCREEN() printf("\033[H\033[J")

int sock;
char username[32];
char estado[32] = "ACTIVO";
char messages_list[MAX_MESSAGES][256];
int messages_count = 0;
pthread_mutex_t history_mutex = PTHREAD_MUTEX_INITIALIZER;
int running = 1;

void add_message_to_message_list(const char *message)
{
    pthread_mutex_lock(&history_mutex);

    if (messages_count < MAX_MESSAGES)
    {
        strncpy(messages_list[messages_count], message, 255);
        messages_list[messages_count][255] = '\0';
        messages_count++;
    }
    else
    {
        for (int i = 0; i < MAX_MESSAGES - 1; i++)
        {
            strcpy(messages_list[i], messages_list[i + 1]);
        }
        strncpy(messages_list[MAX_MESSAGES - 1], message, 255);
        messages_list[MAX_MESSAGES - 1][255] = '\0';
    }

    pthread_mutex_unlock(&history_mutex);
}

void show_all_messages()
{
    CLEAR_SCREEN();
    pthread_mutex_lock(&history_mutex);
    for (int i = 0; i < messages_count; i++)
    {
        printf("%s\n", messages_list[i]);
    }
    pthread_mutex_unlock(&history_mutex);

    printf("\n> ");
    fflush(stdout);
}

void send_json(const char *json_str)
{
    int send_status = send(sock, json_str, strlen(json_str), 0);

    if (send_status < 0)
    {
        perror("Error al enviar el mensaje como json");
    }
}

void handle_special_commads(char *message, const char *username, const char *server_ip)
{
    struct json_object *json_msg;
    const char *json_str;

    // Verificar si son comandos con / o es un mensaje normal
    if (message[0] == '/')
    {
        if (strncmp(message, "/BROADCAST", 10) == 0)
        {
            char mensaje[256];
            snprintf(mensaje, sizeof(mensaje), "%s", message + 11); // Obtener el mensaje después del comando

            json_msg = json_object_new_object();
            json_object_object_add(json_msg, "accion", json_object_new_string("BROADCAST"));
            json_object_object_add(json_msg, "nombre_emisor", json_object_new_string(username));
            json_object_object_add(json_msg, "mensaje", json_object_new_string(mensaje));
            json_str = json_object_to_json_string(json_msg);
            send_json(json_str);
            json_object_put(json_msg);

            char history_msg[256];
            snprintf(history_msg, sizeof(history_msg), "[%s] (BROADCAST): %s", username, mensaje);
            add_message_to_message_list(history_msg);
            show_all_messages();
        }
        else if (strncmp(message, "/DM", 3) == 0)
        {
            // Comando de DM (Direct Message)
            char destinatario[256], mensaje[256];
            sscanf(message + 4, "%s %[^\n]", destinatario, mensaje);

            // Crear JSON para DM
            json_msg = json_object_new_object();
            json_object_object_add(json_msg, "accion", json_object_new_string("DM"));
            json_object_object_add(json_msg, "nombre_emisor", json_object_new_string(username));
            json_object_object_add(json_msg, "nombre_destinatario", json_object_new_string(destinatario));
            json_object_object_add(json_msg, "mensaje", json_object_new_string(mensaje));
            json_str = json_object_to_json_string(json_msg);
            send_json(json_str);
            json_object_put(json_msg);

            // Agregar a la historia
            char history_msg[256];
            snprintf(history_msg, sizeof(history_msg), "PRIVADO A [%s]: %s", destinatario, mensaje);
            add_message_to_message_list(history_msg);
            show_all_messages();
        }
        else if (strncmp(message, "/LISTA", 6) == 0)
        {
            // Comando para solicitar la lista de usuarios
            json_msg = json_object_new_object();
            json_object_object_add(json_msg, "accion", json_object_new_string("LISTA"));
            json_object_object_add(json_msg, "nombre_usuario", json_object_new_string(username));
            json_str = json_object_to_json_string(json_msg);
            send_json(json_str);
            json_object_put(json_msg);
        }
        else if (strncmp(message, "/ESTADO", 7) == 0)
        {
            // Comando para cambiar estado
            char nuevo_estado[32];
            sscanf(message + 8, "%s", nuevo_estado);

            if (strcmp(nuevo_estado, "ACTIVO") != 0 &&
                strcmp(nuevo_estado, "OCUPADO") != 0 &&
                strcmp(nuevo_estado, "INACTIVO") != 0)
            {
                add_message_to_message_list("ERROR: Solo puedes usar los estados: ACTIVO, OCUPADO o INACTIVO.");
                show_all_messages();
                return;
            }

            json_msg = json_object_new_object();
            json_object_object_add(json_msg, "tipo", json_object_new_string("ESTADO"));
            json_object_object_add(json_msg, "usuario", json_object_new_string(username));
            json_object_object_add(json_msg, "estado", json_object_new_string(nuevo_estado));
            json_str = json_object_to_json_string(json_msg);
            send_json(json_str);
            json_object_put(json_msg);

            strcpy(estado, nuevo_estado);

            char history_msg[256];
            snprintf(history_msg, sizeof(history_msg), "Tu estado ha cambiado a: %s", nuevo_estado);
            add_message_to_message_list(history_msg);
            show_all_messages();
        }
        else if (strncmp(message, "/MOSTRAR", 8) == 0)
        {
            char usuario_buscado[256];
            sscanf(message + 9, "%s", usuario_buscado);

            json_msg = json_object_new_object();
            json_object_object_add(json_msg, "tipo", json_object_new_string("MOSTRAR"));
            json_object_object_add(json_msg, "usuario", json_object_new_string(usuario_buscado));
            json_str = json_object_to_json_string(json_msg);

            send_json(json_str);
            json_object_put(json_msg);
        }
    }
    else
    {

        // ENVIO DE MENSAJE NORMAL
        json_msg = json_object_new_object();
        json_object_object_add(json_msg, "accion", json_object_new_string("BROADCAST"));
        json_object_object_add(json_msg, "nombre_emisor", json_object_new_string(username));
        json_object_object_add(json_msg, "mensaje", json_object_new_string(message));
        json_str = json_object_to_json_string(json_msg);
        send_json(json_str);
        json_object_put(json_msg);

        char history_msg[256];
        snprintf(history_msg, sizeof(history_msg), "[%s]: %s", username, message);
        add_message_to_message_list(history_msg);
        show_all_messages();
    }
}

void *receive_thread(void *arg)
{
    char buffer[1024];
    int read_size;

    while ((read_size = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[read_size] = '\0';

        struct json_object *parsed_json = json_tokener_parse(buffer);
        if (!parsed_json)
        {
            printf("JSON inválido recibido\n");
            continue;
        }

        struct json_object *respuesta, *tipo, *accion;
        const char *respuesta_str = NULL;
        const char *tipo_str = NULL;
        const char *accion_str = NULL;

        // Verificar si hay respuesta
        if (json_object_object_get_ex(parsed_json, "respuesta", &respuesta))
        {
            respuesta_str = json_object_get_string(respuesta);
            if (strcmp(respuesta_str, "ERROR") == 0)
            {
                struct json_object *razon;
                if (json_object_object_get_ex(parsed_json, "razon", &razon))
                {
                    char history_msg[256];
                    snprintf(history_msg, sizeof(history_msg), "Error: %s", json_object_get_string(razon));
                    add_message_to_message_list(history_msg);
                    show_all_messages();
                }
            }
        }

        // Verificar si hay tipo
        if (json_object_object_get_ex(parsed_json, "tipo", &tipo))
        {
            tipo_str = json_object_get_string(tipo);
            if (strcmp(tipo_str, "LISTA") == 0)
            {
                struct json_object *usuarios;
                if (json_object_object_get_ex(parsed_json, "usuarios", &usuarios))
                {
                    int num_usuarios = json_object_array_length(usuarios);
                    add_message_to_message_list("Usuarios conectados:");
                    for (int i = 0; i < num_usuarios; i++)
                    {
                        struct json_object *usuario_obj = json_object_array_get_idx(usuarios, i);
                        struct json_object *nombre, *estado_usr;
                        const char *nombre_str, *estado_str;

                        if (json_object_object_get_ex(usuario_obj, "nombre", &nombre) &&
                            json_object_object_get_ex(usuario_obj, "estado", &estado_usr))
                        {
                            nombre_str = json_object_get_string(nombre);
                            estado_str = json_object_get_string(estado_usr);

                            char history_msg[256];
                            snprintf(history_msg, sizeof(history_msg), "%s [%s]", nombre_str, estado_str);
                            add_message_to_message_list(history_msg);
                        }
                    }
                    show_all_messages();
                }
            }
            else if (strcmp(tipo_str, "INFO_USUARIO") == 0)
            {
                struct json_object *usuario, *estado_usr, *direccionIP;
                if (json_object_object_get_ex(parsed_json, "usuario", &usuario) &&
                    json_object_object_get_ex(parsed_json, "estado", &estado_usr) &&
                    json_object_object_get_ex(parsed_json, "direccionIP", &direccionIP))
                {

                    add_message_to_message_list("Información de un usuario: ");
                    char history_msg[256];
                    snprintf(history_msg, sizeof(history_msg), "Usuario: %s", json_object_get_string(usuario));
                    add_message_to_message_list(history_msg);
                    snprintf(history_msg, sizeof(history_msg), "Estado: %s", json_object_get_string(estado_usr));
                    add_message_to_message_list(history_msg);
                    snprintf(history_msg, sizeof(history_msg), "IP: %s", json_object_get_string(direccionIP));
                    add_message_to_message_list(history_msg);
                    show_all_messages();
                }
            }
        }

        // Verificar si hay acción
        if (json_object_object_get_ex(parsed_json, "accion", &accion))
        {
            accion_str = json_object_get_string(accion);

            if (strcmp(accion_str, "BROADCAST") == 0)
            {
                struct json_object *emisor, *mensaje;
                if (json_object_object_get_ex(parsed_json, "nombre_emisor", &emisor) &&
                    json_object_object_get_ex(parsed_json, "mensaje", &mensaje))
                {

                    const char *emisor_str = json_object_get_string(emisor);
                    const char *mensaje_str = json_object_get_string(mensaje);

                    // No mostrar los mensajes propios ya que ya los mostramos al enviarlos
                    if (strcmp(emisor_str, username) != 0)
                    {
                        char history_msg[256];
                        snprintf(history_msg, sizeof(history_msg), "[%s]: %s", emisor_str, mensaje_str);
                        add_message_to_message_list(history_msg);
                        show_all_messages();
                    }
                }
            }
            else if (strcmp(accion_str, "DM") == 0)
            {
                struct json_object *emisor, *mensaje;
                if (json_object_object_get_ex(parsed_json, "nombre_emisor", &emisor) &&
                    json_object_object_get_ex(parsed_json, "mensaje", &mensaje))
                {

                    const char *emisor_str = json_object_get_string(emisor);
                    const char *mensaje_str = json_object_get_string(mensaje);

                    char history_msg[256];
                    snprintf(history_msg, sizeof(history_msg), "PRIVADO DE [%s]: %s", emisor_str, mensaje_str);
                    add_message_to_message_list(history_msg);
                    show_all_messages();
                }
            }
        }

        json_object_put(parsed_json);
    }

    if (read_size == 0)
    {
        add_message_to_message_list("Servidor desconectado.");
    }
    else if (read_size == -1)
    {
        perror("Error recibiendo datos");
    }

    running = 0;
    return NULL;
}

int main(int argc, char *argv[])
{
    struct sockaddr_in server;
    pthread_t recv_thread;
    char message[1024];

    if (argc != 4)
    {
        printf("Uso: %s <nombre_usuario> <servidor_ip> <puerto>\n", argv[0]);
        return 1;
    }

    // Inicialización
    strncpy(username, argv[1], sizeof(username));
    char *server_ip = argv[2];
    int port = atoi(argv[3]);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("No se pudo crear socket");
        return 1;
    }

    server.sin_addr.s_addr = inet_addr(server_ip);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("Error de conexión");
        return 1;
    }

    printf("Conectado al servidor como: %s\n", username);

    struct json_object *json_register = json_object_new_object();
    json_object_object_add(json_register, "tipo", json_object_new_string("REGISTRO"));
    json_object_object_add(json_register, "usuario", json_object_new_string(username));

    struct sockaddr_in local_address;
    socklen_t address_length = sizeof(local_address);
    if (getsockname(sock, (struct sockaddr *)&local_address, &address_length) == 0)
    {
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &local_address.sin_addr, client_ip, INET_ADDRSTRLEN);
        json_object_object_add(json_register, "direccionIP", json_object_new_string(client_ip));
    }
    else
    {
        perror("Error al obtener la dirección IP del cliente");
    }

    const char *json_str = json_object_to_json_string(json_register);
    if (send(sock, json_str, strlen(json_str), 0) < 0)
    {
        perror("Error al enviar registro");
        return 1;
    }

    json_object_put(json_register);

    // Esperar respuesta del servidor
    char buffer[1024];
    int read_size = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (read_size <= 0)
    {
        perror("Error recibiendo respuesta de registro");
        return 1;
    }

    buffer[read_size] = '\0';
    struct json_object *response = json_tokener_parse(buffer);
    struct json_object *resp_val;

    if (json_object_object_get_ex(response, "respuesta", &resp_val))
    {
        if (strcmp(json_object_get_string(resp_val), "ERROR") == 0)
        {
            struct json_object *razon;
            if (json_object_object_get_ex(response, "razon", &razon))
            {
                printf("Error al registrar: %s\n", json_object_get_string(razon));
                json_object_put(response);
                return 1;
            }
        }
    }

    json_object_put(response);

    if (pthread_create(&recv_thread, NULL, receive_thread, NULL) < 0)
    {
        perror("Error al crear thread de recepción");
        return 1;
    }

    while (running)
    {
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = 0;

        if (strlen(message) > 0)
        {
            handle_special_commads(message, username, server_ip);
        }
    }

    close(sock);

    pthread_join(recv_thread, NULL);

    return 0;
}