#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ncurses.h>
#include <openssl/aes.h>

#define BUFFER_SIZE 1024
const unsigned char AES_KEY[16] = "1234567890123456"; // Clave AES (Ejemplo)

int socket_fd;
struct sockaddr_in server_addr;

void encrypt_message(const char *input, unsigned char *output)
{
    AES_KEY encryptKey;
    AES_set_encrypt_key(AES_KEY, 128, &encryptKey);
    AES_encrypt((unsigned char *)input, output, &encryptKey);
}

void decrypt_message(const unsigned char *input, char *output)
{
    AES_KEY decryptKey;
    AES_set_decrypt_key(AES_KEY, 128, &decryptKey);
    AES_decrypt(input, (unsigned char *)output, &decryptKey);
}

void init_ncurses()
{
    initscr();
    cbreak();
    noecho();
    scrollok(stdscr, TRUE);
}

void display_message(const char *msg)
{
    printw("%s\n", msg);
    refresh();
}

void *receive_messages(void *arg)
{
    char buffer[BUFFER_SIZE];
    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(socket_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
        {
            display_message("Desconectado del servidor. Intentando reconectar...");
            close(socket_fd);
            reconnect();
        }
        else
        {
            display_message(buffer);
        }
    }
    return NULL;
}

void reconnect()
{
    while (1)
    {
        socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
        {
            display_message("Reconectado al servidor.");
            return;
        }
        sleep(3);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Uso: %s <IP del servidor> <puerto>\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("Error al conectar con el servidor.\n");
        return 1;
    }

    init_ncurses();
    pthread_t receive_thread;
    pthread_create(&receive_thread, NULL, receive_messages, NULL);

    char input[BUFFER_SIZE];
    while (1)
    {
        getstr(input);
        send(socket_fd, input, strlen(input), 0);
    }

    endwin();
    close(socket_fd);
    return 0;
}