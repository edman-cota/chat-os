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
pthread_mutex_t clientes_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex para proteger el acceso a la lista de clientes

// enviar mensaje a todos menos al remitente
void broadcast(const char *mensaje, int remitente)
{
	pthread_mutex_lock(&clientes_mutex); // Bloqueamos acceso a la lista de clientes

	for (int i = 0; i < MAX_CLIENTES; i++)
	{
		if (clientes[i].socket != 0 && clientes[i].socket != remitente)
		{
			send(clientes[i].socket, mensaje, strlen(mensaje), 0);
		}
	}

	pthread_mutex_unlock(&clientes_mutex); // Desbloqueamos el acceso
}

void enviar_mensaje_privado(const char *remitente, const char *destinatario, const char *mensaje, int remitente_socket)
{
	pthread_mutex_lock(&clientes_mutex);
	int encontrado = 0;

	for (int i = 0; i < MAX_CLIENTES; i++)
	{
		if (clientes[i].socket != 0 && strcmp(clientes[i].nombre, destinatario) == 0)
		{
			char buffer[BUFFER_SIZE];
			snprintf(buffer, sizeof(buffer), "[Privado de %s]: %s\n", remitente, mensaje);
			send(clientes[i].socket, buffer, strlen(buffer), 0);
			encontrado = 1;
			break;
		}
	}

	if (!encontrado)
	{
		char error_msg[BUFFER_SIZE];
		snprintf(error_msg, sizeof(error_msg), "Usuario '%s' no encontrado.\n", destinatario);
		send(remitente_socket, error_msg, strlen(error_msg), 0);
	}

	pthread_mutex_unlock(&clientes_mutex);
}

void manejar_cliente(void *arg)
{
	int cliente_socket = *(int *)arg;
	char buffer[BUFFER_SIZE];
	char nombre[50];

	recv(cliente_socket, nombre, sizeof(nombre), 0);

	pthread_mutex_lock(&clientes_mutex);
	for (int i = 0; i < MAX_CLIENTES; i++)
	{
		if (clientes[i].socket == 0)
		{
			clientes[i].socket = cliente_socket;
			strcpy(clientes[i].nombre, nombre);
			strcpy(clientes[i].status, "ACTIVO");
			break;
		}
	}
	pthread_mutex_unlock(&clientes_mutex);

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
			if (sscanf(buffer, "/msg %s %[^\n]", destinatario, mensaje) == 2)
			{
				enviar_mensaje_privado(nombre, destinatario, mensaje, cliente_socket);
			}
			else
			{
				send(cliente_socket, "Uso incorrecto. Formato: /msg <usuario> <mensaje>\n", 50, 0);
			}
		}
		else if (strncmp(buffer, "/status", 7) == 0)
		{
			char nuevo_status[10];
			if (sscanf(buffer, "/status %s", nuevo_status) == 1)
			{
				pthread_mutex_lock(&clientes_mutex);
				for (int i = 0; i < MAX_CLIENTES; i++)
				{
					if (clientes[i].socket == cliente_socket)
					{
						strcpy(clientes[i].status, nuevo_status);
						break;
					}
				}
				pthread_mutex_unlock(&clientes_mutex);
			}
		}
		else
		{
			broadcast(buffer, cliente_socket);
		}
	}

	close(cliente_socket);
	pthread_mutex_lock(&clientes_mutex);
	for (int i = 0; i < MAX_CLIENTES; i++)
	{
		if (clientes[i].socket == cliente_socket)
		{
			clientes[i].socket = 0;
			break;
		}
	}
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

	if (bind(servidor_socket, (struct sockaddr *)&servidor_addr, sizeof(servidor_addr)) < 0)
	{
		perror("Error al hacer bind");
		close(servidor_socket);
		return 1;
	}

	if (listen(servidor_socket, MAX_CLIENTES) < 0)
	{
		perror("Error al escuchar conexiones");
		close(servidor_socket);
		return 1;
	}

	printf("Servidor en ejecución en el puerto %s...\n", argv[1]);

	while (1)
	{
		cliente_socket = accept(servidor_socket, (struct sockaddr *)&cliente_addr, &cliente_len);
		if (cliente_socket < 0)
		{
			perror("Error al aceptar la conexión del cliente");
			continue;
		}

		printf("Cliente conectado desde %s:%d\n", inet_ntoa(cliente_addr.sin_addr), ntohs(cliente_addr.sin_port));

		pthread_t hilo;
		if (pthread_create(&hilo, NULL, (void *)manejar_cliente, &cliente_socket) != 0)
		{
			perror("Error al crear el hilo para el cliente");
			close(cliente_socket);
		}
	}

	close(servidor_socket);
	return 0;
}