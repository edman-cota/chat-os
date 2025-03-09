#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_CLIENTES 10
#define BUFFER_SIZE 1024

typedef struct
{
	int socket;
	char nombre[50];
	char status[10];
	struct sockaddr_in direccion;
} Cliente;

Cliente clientes[MAX_CLIENTES];
pthread_mutex_t clientes_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast(const char *mensaje, int remitente)
{
	pthread_mutex_lock(&clientes_mutex);
	for (int i = 0; i < MAX_CLIENTES; i++)
	{
		if (clientes[i].socket != 0 && clientes[i].socket != remitente)
		{
			send(clientes[i].socket, mensaje, strlen(mensaje), 0);
		}
	}
	pthread_mutex_unlock(&clientes_mutex);
}

void enviar_mensaje_privado(const char *destinatario, const char *mensaje, int remitente)
{
	pthread_mutex_lock(&clientes_mutex);
	for (int i = 0; i < MAX_CLIENTES; i++)
	{
		if (clientes[i].socket != 0 && strcmp(clientes[i].nombre, destinatario) == 0)
		{
			send(clientes[i].socket, mensaje, strlen(mensaje), 0);
			pthread_mutex_unlock(&clientes_mutex);
			return;
		}
	}
	pthread_mutex_unlock(&clientes_mutex);
}

void manejar_cliente(void *arg)
{
	int cliente_socket = *(int *)arg;
	char buffer[BUFFER_SIZE];
	char nombre[50];

	recv(cliente_socket, nombre, sizeof(nombre), 0);
	strcpy(clientes[cliente_socket].nombre, nombre);
	strcpy(clientes[cliente_socket].status, "ACTIVO");

	snprintf(buffer, sizeof(buffer), "%s se ha conectado.\n", nombre);
	broadcast(buffer, cliente_socket);

	while (1)
	{
		memset(buffer, 0, BUFFER_SIZE);
		int bytes_recibidos = recv(cliente_socket, buffer, BUFFER_SIZE, 0);
		if (bytes_recibidos <= 0)
		{
			break;
		}

		if (strncmp(buffer, "/users", 6) == 0)
		{
			pthread_mutex_lock(&clientes_mutex);
			char lista[BUFFER_SIZE] = "Usuarios conectados:\n";
			for (int i = 0; i < MAX_CLIENTES; i++)
			{
				if (clientes[i].socket != 0)
				{
					strcat(lista, clientes[i].nombre);
					strcat(lista, "\n");
				}
			}
			send(cliente_socket, lista, strlen(lista), 0);
			pthread_mutex_unlock(&clientes_mutex);
		}
		else if (strncmp(buffer, "/msg", 4) == 0)
		{
			char destinatario[50], mensaje[BUFFER_SIZE];
			sscanf(buffer, "/msg %s %[^\n]", destinatario, mensaje);
			enviar_mensaje_privado(destinatario, mensaje, cliente_socket);
		}
		else if (strncmp(buffer, "/status", 7) == 0)
		{
			char nuevo_status[10];
			sscanf(buffer, "/status %s", nuevo_status);
			pthread_mutex_lock(&clientes_mutex);
			strcpy(clientes[cliente_socket].status, nuevo_status);
			pthread_mutex_unlock(&clientes_mutex);
		}
		else
		{
			broadcast(buffer, cliente_socket);
		}
	}

	close(cliente_socket);
	pthread_mutex_lock(&clientes_mutex);
	clientes[cliente_socket].socket = 0;
	pthread_mutex_unlock(&clientes_mutex);
	snprintf(buffer, sizeof(buffer), "%s se ha desconectado.\n", nombre);
	broadcast(buffer, cliente_socket);
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Uso: %s <puerto>\n", argv[0]);
		return 1;
	}

	int servidor_socket, cliente_socket;
	struct sockaddr_in servidor_addr, cliente_addr;
	socklen_t cliente_len = sizeof(cliente_addr);

	servidor_socket = socket(AF_INET, SOCK_STREAM, 0);
	servidor_addr.sin_family = AF_INET;
	servidor_addr.sin_addr.s_addr = INADDR_ANY;
	servidor_addr.sin_port = htons(atoi(argv[1]));

	bind(servidor_socket, (struct sockaddr *)&servidor_addr, sizeof(servidor_addr));
	listen(servidor_socket, MAX_CLIENTES);

	printf("Servidor en ejecución en el puerto %s...\n", argv[1]);

	while (1)
	{
		cliente_socket = accept(servidor_socket, (struct sockaddr *)&cliente_addr, &cliente_len);
		pthread_mutex_lock(&clientes_mutex);
		for (int i = 0; i < MAX_CLIENTES; i++)
		{
			if (clientes[i].socket == 0)
			{
				clientes[i].socket = cliente_socket;
				clientes[i].direccion = cliente_addr;
				pthread_t hilo;
				pthread_create(&hilo, NULL, (void *)manejar_cliente, &cliente_socket);
				break;
			}
		}
		pthread_mutex_unlock(&clientes_mutex);
	}

	close(servidor_socket);
	return 0;
}