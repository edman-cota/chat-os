#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <process.h>
#define close(sock) closesocket(sock)
#define CLEAR_SCREEN() system("cls")
typedef SOCKET socket_t;
#else
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#define CLEAR_SCREEN() printf("\033[H\033[J")
typedef int socket_t;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <json-c/json.h>

#define MAX_CLIENTS 100
#define STATUS_LENGTH 10

typedef struct
{
	socket_t socket;
	char username[50];
	char ip_address[16];
	char estado[STATUS_LENGTH];
	time_t last_activity;
} Client;

// Declaraciones de funciones (prototipos)
void *handle_client(void *socket_desc);
void handle_register_client(struct json_object *parsed_json, socket_t sock);
void broadcast_message(const char *message, const char *emisor);
void send_direct_message(const char *receiver, const char *message, const char *emisor);
void list_connected_users(socket_t socket);
void handle_status_change(struct json_object *parsed_json, socket_t sock);
void handle_mostrar(struct json_object *parsed_json, socket_t sock);
void remove_client_from_server(socket_t socket);
void send_json_response(socket_t socket, const char *status, const char *key, const char *message);
int register_new_client(socket_t socket, const char *username, const char *ip_address);

Client *clients[MAX_CLIENTS];
#ifdef _WIN32
HANDLE clients_mutex;
#else
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

void *handle_client(void *socket_desc)
{
	socket_t sock = *(socket_t *)socket_desc;
	free(socket_desc);
	char buffer[1024];
	int read_size;

	read_size = recv(sock, buffer, sizeof(buffer), 0);
	if (read_size <= 0)
	{
		close(sock);
		return NULL;
	}
	buffer[read_size] = '\0';

	struct json_object *parsed_json = json_tokener_parse(buffer);
	struct json_object *tipo;

	if (json_object_object_get_ex(parsed_json, "tipo", &tipo) &&
		strcmp(json_object_get_string(tipo), "REGISTRO") == 0)
	{
		handle_register_client(parsed_json, sock);
	}
	else
	{
		send_json_response(sock, "ERROR", "razon", "Registro inválido");
		close(sock);
		json_object_put(parsed_json);
		return NULL;
	}
	json_object_put(parsed_json);

	while ((read_size = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0)
	{
		buffer[read_size] = '\0';
		printf("Mensaje recibido en el servidor: %s\n", buffer);

		struct json_object *msg_json = json_tokener_parse(buffer);
		if (!msg_json)
		{
			printf("JSON inválido recibido\n");
			continue;
		}

		struct json_object *tipo, *accion;
		const char *tipo_str = NULL;
		const char *accion_str = NULL;

		if (json_object_object_get_ex(msg_json, "tipo", &tipo))
		{
			tipo_str = json_object_get_string(tipo);
		}

		if (json_object_object_get_ex(msg_json, "accion", &accion))
		{
			accion_str = json_object_get_string(accion);
		}

		if (tipo_str)
		{
			if (strcmp(tipo_str, "MENSAJE") == 0)
			{
				struct json_object *usuario, *mensaje;
				if (json_object_object_get_ex(msg_json, "usuario", &usuario) &&
					json_object_object_get_ex(msg_json, "mensaje", &mensaje))
				{
					printf("[MENSAJE] %s: %s\n", json_object_get_string(usuario), json_object_get_string(mensaje));
				}
			}
			else if (strcmp(tipo_str, "REGISTRO") == 0)
			{
				handle_register_client(msg_json, sock);
			}
			else if (strcmp(tipo_str, "ESTADO") == 0)
			{
				handle_status_change(msg_json, sock);
			}
			else if (strcmp(tipo_str, "MOSTRAR") == 0)
			{
				handle_mostrar(msg_json, sock);
			}
		}
		else if (accion_str)
		{
			if (strcmp(accion_str, "BROADCAST") == 0)
			{
				struct json_object *emisor, *mensaje;
				if (json_object_object_get_ex(msg_json, "nombre_emisor", &emisor) &&
					json_object_object_get_ex(msg_json, "mensaje", &mensaje))
				{
					broadcast_message(json_object_get_string(mensaje), json_object_get_string(emisor));
				}
			}
			else if (strcmp(accion_str, "DM") == 0)
			{
				struct json_object *emisor, *destinatario, *mensaje;
				if (json_object_object_get_ex(msg_json, "nombre_emisor", &emisor) &&
					json_object_object_get_ex(msg_json, "nombre_destinatario", &destinatario) &&
					json_object_object_get_ex(msg_json, "mensaje", &mensaje))
				{
					send_direct_message(json_object_get_string(destinatario),
										json_object_get_string(mensaje),
										json_object_get_string(emisor));
				}
			}
			else if (strcmp(accion_str, "LISTA") == 0)
			{
				list_connected_users(sock);
			}
		}

		json_object_put(msg_json);
	}

	remove_client_from_server(sock);
	printf("Cliente desconectado del servidor\n");
	close(sock);
	return NULL;
}

handle_register_client(struct json_object *parsed_json, int sock)
{
	struct json_object *usuario;
	if (json_object_object_get_ex(parsed_json, "usuario", &usuario))
	{
		const char *username = json_object_get_string(usuario);

		struct sockaddr_in peer_address;
		socklen_t peer_address_len = sizeof(peer_address);
		getpeername(sock, (struct sockaddr *)&peer_address, &peer_address_len);
		char client_ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &peer_address.sin_addr, client_ip, INET_ADDRSTRLEN);

		if (register_new_client(sock, username, client_ip))
		{
			send_json_response(sock, "OK", "respuesta", "Registro exitoso");
		}
		else
		{
			send_json_response(sock, "ERROR", "razon", "Usuario/IP duplicado");
		}
	}
	else
	{
		send_json_response(sock, "ERROR", "razon", "Campos faltantes");
	}
}

void broadcast_message(const char *message, const char *emisor)
{
	pthread_mutex_lock(&clients_mutex);
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i] != NULL)
		{
			struct json_object *json_msg = json_object_new_object();
			json_object_object_add(json_msg, "accion", json_object_new_string("BROADCAST"));
			json_object_object_add(json_msg, "nombre_emisor", json_object_new_string(emisor));
			json_object_object_add(json_msg, "mensaje", json_object_new_string(message));

			const char *json_str = json_object_to_json_string(json_msg);
			send(clients[i]->socket, json_str, strlen(json_str), 0);

			json_object_put(json_msg);
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

void send_direct_message(const char *receiver, const char *message, const char *emisor)
{
	pthread_mutex_lock(&clients_mutex);
	int found = 0;
	int emisor_socket = -1; // Declaración de emisor_socket
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i] != NULL && strcmp(clients[i]->username, receiver) == 0)
		{
			struct json_object *json_msg = json_object_new_object();
			json_object_object_add(json_msg, "accion", json_object_new_string("DM"));
			json_object_object_add(json_msg, "nombre_emisor", json_object_new_string(emisor));
			json_object_object_add(json_msg, "mensaje", json_object_new_string(message));

			const char *json_str = json_object_to_json_string(json_msg);
			send(clients[i]->socket, json_str, strlen(json_str), 0);

			json_object_put(json_msg);
			found = 1;
			break;
		}
		// Guarda el socket del emisor para usarlo más adelante
		if (clients[i] != NULL && strcmp(clients[i]->username, emisor) == 0)
		{
			emisor_socket = clients[i]->socket;
		}
	}
	pthread_mutex_unlock(&clients_mutex);

	if (!found)
	{
		// Notificar al emisor que el receptor no existe
		struct json_object *json_err = json_object_new_object();
		json_object_object_add(json_err, "respuesta", json_object_new_string("ERROR"));
		json_object_object_add(json_err, "razon", json_object_new_string("Usuario no encontrado"));
		const char *err_str = json_object_to_json_string(json_err);
		send(emisor_socket, err_str, strlen(err_str), 0); // Usar emisor_socket aquí
		json_object_put(json_err);
	}
}

void list_connected_users(socket_t socket)
{
	pthread_mutex_lock(&clients_mutex);

	struct json_object *json_msg = json_object_new_object();
	json_object_object_add(json_msg, "tipo", json_object_new_string("LISTA"));

	struct json_object *usuarios = json_object_new_array();
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i] != NULL && clients[i]->username != NULL)
		{
			// Crear un objeto para cada usuario con su nombre y estado
			struct json_object *usuario_obj = json_object_new_object();
			json_object_object_add(usuario_obj, "nombre", json_object_new_string(clients[i]->username));
			json_object_object_add(usuario_obj, "estado", json_object_new_string(clients[i]->estado));
			json_object_array_add(usuarios, usuario_obj);
		}
	}
	json_object_object_add(json_msg, "usuarios", usuarios);

	const char *json_str = json_object_to_json_string(json_msg);

	char buffer[1024];
	strncpy(buffer, json_str, sizeof(buffer) - 1);
	buffer[sizeof(buffer) - 1] = '\0';

	json_object_put(json_msg);
	pthread_mutex_unlock(&clients_mutex);

	send(socket, buffer, strlen(buffer), 0);
}

void handle_status_change(struct json_object *parsed_json, int sock)
{
	struct json_object *usuario, *estado;
	if (json_object_object_get_ex(parsed_json, "usuario", &usuario) &&
		json_object_object_get_ex(parsed_json, "estado", &estado))
	{

		const char *user = json_object_get_string(usuario);
		const char *new_estado = json_object_get_string(estado);

		pthread_mutex_lock(&clients_mutex);
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (clients[i] != NULL && strcmp(clients[i]->username, user) == 0)
			{
				if (strcmp(clients[i]->estado, new_estado) == 0)
				{
					send_json_response(sock, "ERROR", "razon", "Estado ya seleccionado");
				}
				else
				{
					strncpy(clients[i]->estado, new_estado, STATUS_LENGTH);
					send_json_response(sock, "OK", "respuesta", "Estado actualizado");
				}
				pthread_mutex_unlock(&clients_mutex);
				return;
			}
		}
		pthread_mutex_unlock(&clients_mutex);
		send_json_response(sock, "ERROR", "razon", "Usuario no encontrado");
	}
	else
	{
		send_json_response(sock, "ERROR", "razon", "Campos faltantes");
	}
}

void handle_mostrar(struct json_object *parsed_json, int sock)
{
	struct json_object *usuario;
	if (json_object_object_get_ex(parsed_json, "usuario", &usuario))
	{
		const char *user = json_object_get_string(usuario);

		pthread_mutex_lock(&clients_mutex);
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (clients[i] != NULL && strcmp(clients[i]->username, user) == 0)
			{
				struct json_object *json_resp = json_object_new_object();
				json_object_object_add(json_resp, "tipo", json_object_new_string("INFO_USUARIO"));
				json_object_object_add(json_resp, "usuario", json_object_new_string(user));
				json_object_object_add(json_resp, "estado", json_object_new_string(clients[i]->estado));
				json_object_object_add(json_resp, "direccionIP", json_object_new_string(clients[i]->ip_address));

				const char *resp_str = json_object_to_json_string(json_resp);
				send(sock, resp_str, strlen(resp_str), 0);

				json_object_put(json_resp);
				pthread_mutex_unlock(&clients_mutex);
				return;
			}
		}
		pthread_mutex_unlock(&clients_mutex);
		send_json_response(sock, "ERROR", "razon", "USUARIO_NO_ENCONTRADO");
	}
	else
	{
		send_json_response(sock, "ERROR", "razon", "Campo usuario faltante");
	}
}

void remove_client_from_server(socket_t socket)
{
	pthread_mutex_lock(&clients_mutex);
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i] != NULL && clients[i]->socket == socket)
		{
			free(clients[i]);
			clients[i] = NULL;
			break;
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

void send_json_response(socket_t socket, const char *status, const char *key, const char *message)
{
	struct json_object *json_resp = json_object_new_object();

	json_object_object_add(json_resp, "respuesta", json_object_new_string(status));
	if (key != NULL && message != NULL)
	{
		json_object_object_add(json_resp, key, json_object_new_string(message));
	}
	const char *resp_str = json_object_to_json_string(json_resp);
	printf("Enviando al cliente: %s\n", resp_str);
	send(socket, resp_str, strlen(resp_str), 0);
	json_object_put(json_resp);
}

int register_new_client(socket_t socket, const char *username, const char *ip_address)
{
	pthread_mutex_lock(&clients_mutex);

	// Verificamos que el nuevo cliente no sea duplicado si
	// Si no es NULL
	// Si el cliente nuevo no tiene el mismo nombre de usuario.
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i] != NULL && strcmp(clients[i]->username, username) == 0)
		{
			pthread_mutex_unlock(&clients_mutex);
			return 0;
		}
	}

	// Registrar nuevo cliente
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i] == NULL)
		{
			clients[i] = malloc(sizeof(Client));
			clients[i]->socket = socket;
			strncpy(clients[i]->username, username, sizeof(clients[i]->username));
			strncpy(clients[i]->ip_address, ip_address, sizeof(clients[i]->ip_address));

			strncpy(clients[i]->estado, "ACTIVO", STATUS_LENGTH);
			pthread_mutex_unlock(&clients_mutex);
			return 1;
		}
	}
	pthread_mutex_unlock(&clients_mutex);
	return 0;
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		fprintf(stderr, "Error al inicializar Winsock\n");
		return 1;
	}
	clients_mutex = CreateMutex(NULL, FALSE, NULL);
#endif

	socket_t server_fd, new_socket;
	struct sockaddr_in address;
	socklen_t addrlen = sizeof(address);
	int port;

	if (argc != 2)
	{
		printf("Uso: %s <puerto>\n", argv[0]);
		return 1;
	}

	port = atoi(argv[1]);
	if (port <= 0 || port > 65535)
	{
		printf("Puerto inválido. Debe ser un número entre 1 y 65535\n");
		return 1;
	}

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		perror("Error en bind");
		return 1;
	}

	listen(server_fd, MAX_CLIENTS);
	printf("Servidor escuchando en puerto %d\n", port);

	while (1)
	{
		new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
		if (new_socket < 0)
		{
			perror("Error aceptando conexión");
			continue;
		}

		printf("Nueva conexión aceptada desde %s:%d\n",
			   inet_ntoa(address.sin_addr), ntohs(address.sin_port));

#ifdef _WIN32
		socket_t *new_sock = malloc(sizeof(socket_t));
		*new_sock = new_socket;
		_beginthreadex(NULL, 0, (void *)handle_client, new_sock, 0, NULL);
#else
		pthread_t thread_id;
		socket_t *new_sock = malloc(sizeof(socket_t));
		*new_sock = new_socket;
		pthread_create(&thread_id, NULL, handle_client, new_sock);
		pthread_detach(thread_id);
#endif
	}

	close(server_fd);
#ifdef _WIN32
	WSACleanup();
	CloseHandle(clients_mutex);
#endif
	return 0;
}