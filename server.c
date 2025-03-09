
#include <stdio.h> include <pthread.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

typedef struct {
     int socket;
    char username[BUFFER_SIZE];
	struct sockaddr_in_address;
} Client;

Clint clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast_message(char *message, int sender_socket);
void remove_client(int client_socket);
void *client_handler(void *arg);

int main(int argc, char *argv[]) {
	if(argc != 2) {
		printf("Uso: %s <puerto>\n", argv[0]);
		return -1;
	}

	int server_socket, client_socket;
	struct sockaddr_in server_addr, client_addr;
	socklen_t  addr_size = sizeof(client_addr);
	pthread_t tid;

    	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(server_socket  == -1) {
		perror("Error al crear el socket del servidor");
		return -1;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_add.sin_port = htons(atoi(argv[1]));

	if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("Error en bind");
		return -1;
	}

	if(listen(server_socket,, MAX_CLIENTS) < 0) {
		perror("Error en listen");
		return -1;
	}

	printf("Servidor iniciando en el puerto %s...\n", argv[1]);

	while(1) {
		client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
		if(client_socket < 0) {
			perror("Error en accept");
			continue;
		}

		pthread_t tid;
		int *new_sock = malloc(sizeof(int));
		*new_sock = client_socket;
		pthread_create(&tid, NULL, client_handler, (void *)new_sock);
	}

	close(server_socket);
	return 0;

}

void *client_handler(void *arg) {
	int client_socket = *(int *) arg;
	free(arg);
	char buffer[BUFFER_SIZE];

	recv(client_socket, buffer, sizeof(buffer), 0);

	pthread_mutex_lock(&clients_mutex);
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if(clients[i].socket == 0) {
			clients[i].socket = client_socket;
			strcpy(clients[i].username, buffer);
			clients[i].status = 0; // ACTIVO
			break;
		}
	}
	pthread_mutex_unlock(&clients_mutex);

	printf("%s se ha conectado\n", buffer);

	while  (1) {
		memset(buffer, 0, BUFFER_SIZE);
		int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);

		if(bytes_received  <= 0) {
			printf("Cliente desconectado\n");
			remove_client(client_socket);
			close(client_socket);
			return NULL;
		}
		broadcast_message(buffer, client_socket);
	}
}

void broadcast_message(char *message, int sender_socket) {
	pthread_mutex_lock(&clients_mutex);
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if(clients[i].socket != 0 && clients[i].socket != sender_socket) {
			send(clients[i].socket, message, strlen(message), 0);
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int client_socket) {
	pthread_mutex_lock(&clients_mutex);
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (clients[i].socket == client_socket) {
			clients[i].socket = 0;
			break;
		}
	}
}
