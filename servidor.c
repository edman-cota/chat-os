#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <json-c/json.h>
#include "server_utils.h"

#define PORT 50213

int main()
{
	int server_fd, new_socket;
	struct sockaddr_in address;
	socklen_t addrlen = sizeof(address);

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);

	bind(server_fd, (struct sockaddr *)&address, sizeof(address));
	listen(server_fd, MAX_CLIENTS);

	printf("Servidor escuchando en puerto %d...\n", PORT);

	while (1)
	{
		printf("Esperando conexiones...\n");
		new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
		if (new_socket < 0)
		{
			perror("Error aceptando conexión");
			continue;
		}

		printf("Nueva conexión aceptada desde %s:%d\n",
			   inet_ntoa(address.sin_addr), ntohs(address.sin_port));

		pthread_t thread_id;
		int *new_sock = malloc(sizeof(int));
		*new_sock = new_socket;
		pthread_create(&thread_id, NULL, handle_client, (void *)new_sock);
		pthread_detach(thread_id);
	}

	close(server_fd);
	return 0;
}