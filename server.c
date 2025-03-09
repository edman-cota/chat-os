#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <openssl/aes.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define PORT 8080

typedef struct
{
	int socket;
	char username[50];
	char status[10];
	struct sockaddr_in address;
} Client;

Client clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
const unsigned char AES_KEY[16] = "1234567890123456"; // Clave AES (Ejemplo)

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

void send_message_to_all(char *message)
{
	pthread_mutex_lock(&clients_mutex);
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].socket != 0)
		{
			send(clients[i].socket, message, strlen(message), 0);
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

void send_user_list(int client_socket)
{
	char user_list[BUFFER_SIZE] = "Usuarios conectados:\n";
	pthread_mutex_lock(&clients_mutex);
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].socket != 0)
		{
			strcat(user_list, clients[i].username);
			strcat(user_list, "\n");
		}
	}
	pthread_mutex_unlock(&clients_mutex);
	send(client_socket, user_list, strlen(user_list), 0);
}

void process_command(char *buffer, int client_socket)
{
	if (strncmp(buffer, "/status ", 8) == 0)
	{
		char status[10];
		sscanf(buffer, "/status %s", status);
		pthread_mutex_lock(&clients_mutex);
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (clients[i].socket == client_socket)
			{
				strcpy(clients[i].status, status);
				break;
			}
		}
		pthread_mutex_unlock(&clients_mutex);
	}
	else if (strncmp(buffer, "/users", 6) == 0)
	{
		send_user_list(client_socket);
	}
	else
	{
		send(client_socket, "Comando no reconocido\n", 23, 0);
	}
}

void *handle_client(void *arg)
{
	int client_socket = *((int *)arg);
	char buffer[BUFFER_SIZE];
	while (1)
	{
		memset(buffer, 0, BUFFER_SIZE);
		int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
		if (bytes_received <= 0)
		{
			break;
		}
		process_command(buffer, client_socket);
	}
	close(client_socket);
	return NULL;
}

int main()
{
	int server_socket, client_socket;
	struct sockaddr_in server_addr, client_addr;
	socklen_t client_len = sizeof(client_addr);

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(PORT);

	bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
	listen(server_socket, 10);

	printf("Servidor escuchando en el puerto %d\n", PORT);

	while (1)
	{
		client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
		pthread_t thread;
		pthread_create(&thread, NULL, handle_client, &client_socket);
	}

	return 0;
}