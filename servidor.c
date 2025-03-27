#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <json-c/json.h>
#include <time.h>

#define MAX_CLIENTES 10
#define BUFFER_SIZE 2048

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

void enviar_respuesta(int socket, const char *tipo, const char *contenido)
{
	struct json_object *respuesta = json_object_new_object();
	json_object_object_add(respuesta, "tipo", json_object_new_string(tipo));
	json_object_object_add(respuesta, "contenido", json_object_new_string(contenido));

	const char *json_str = json_object_to_json_string(respuesta);
	send(socket, json_str, strlen(json_str), 0);

	json_object_put(respuesta);
}

void enviar_mensaje_privado(const char *remitente, const char *destinatario, const char *mensaje, int remitente_socket)
{
	pthread_mutex_lock(&clientes_mutex);
	int encontrado = 0;

	for (int i = 0; i < MAX_CLIENTES; i++)
	{
		if (clientes[i].socket != 0 && strcmp(clientes[i].nombre, destinatario) == 0)
		{
			struct json_object *msg_json = json_object_new_object();
			json_object_object_add(msg_json, "tipo", json_object_new_string("privado"));
			json_object_object_add(msg_json, "remitente", json_object_new_string(remitente));
			json_object_object_add(msg_json, "contenido", json_object_new_string(mensaje));

			const char *json_str = json_object_to_json_string(msg_json);
			send(clientes[i].socket, json_str, strlen(json_str), 0);

			json_object_put(msg_json);
			encontrado = 1;
			break;
		}
	}

	if (!encontrado)
	{
		enviar_respuesta(remitente_socket, "error", "Usuario no encontrado");
	}

	pthread_mutex_unlock(&clientes_mutex);
}

void manejar_cliente(void *arg)
{
	int cliente_socket = *(int *)arg;
	char buffer[BUFFER_SIZE];

	// Recibir mensaje de conexión
	recv(cliente_socket, buffer, BUFFER_SIZE, 0);

	struct json_object *parsed_json = json_tokener_parse(buffer);
	if (!parsed_json)
	{
#ifdef _WIN32
		closesocket(cliente_socket);
#else
		close(cliente_socket);
#endif
		return;
	}

	json_object *usuario_obj, *tipo_obj;
	json_object_object_get_ex(parsed_json, "usuario", &usuario_obj);
	json_object_object_get_ex(parsed_json, "tipo", &tipo_obj);

	const char *nombre = json_object_get_string(usuario_obj);
	const char *tipo = json_object_get_string(tipo_obj);

	// Registrar cliente
	pthread_mutex_lock(&clientes_mutex);
	for (int i = 0; i < MAX_CLIENTES; i++)
	{
		if (clientes[i].socket == 0)
		{
			clientes[i].socket = cliente_socket;
			strncpy(clientes[i].nombre, nombre, sizeof(clientes[i].nombre) - 1);
			strncpy(clientes[i].status, "ACTIVO", sizeof(clientes[i].status) - 1);
			break;
		}
	}
	pthread_mutex_unlock(&clientes_mutex);

	// Notificar conexión
	struct json_object *conexion_json = json_object_new_object();
	json_object_object_add(conexion_json, "tipo", json_object_new_string("sistema"));

	char temp_msg[100];
	snprintf(temp_msg, sizeof(temp_msg), "%s se ha conectado", nombre);
	json_object_object_add(conexion_json, "contenido", json_object_new_string(temp_msg));

	broadcast(json_object_to_json_string(conexion_json), cliente_socket);
	json_object_put(conexion_json);
	json_object_put(parsed_json);

	while (1)
	{
		memset(buffer, 0, BUFFER_SIZE);
		int bytes_recibidos = recv(cliente_socket, buffer, BUFFER_SIZE, 0);
		if (bytes_recibidos <= 0)
			break;

		parsed_json = json_tokener_parse(buffer);
		if (!parsed_json)
			continue;

		json_object *tipo_obj, *contenido_obj;
		json_object_object_get_ex(parsed_json, "tipo", &tipo_obj);
		json_object_object_get_ex(parsed_json, "contenido", &contenido_obj);
		const char *tipo = json_object_get_string(tipo_obj);
		const char *contenido = json_object_get_string(contenido_obj);

		if (strcmp(tipo, "comando") == 0)
		{
			if (strncmp(contenido, "/users", 6) == 0)
			{
				struct json_object *respuesta = json_object_new_object();
				struct json_object *usuarios_array = json_object_new_array();

				pthread_mutex_lock(&clientes_mutex);
				for (int i = 0; i < MAX_CLIENTES; i++)
				{
					if (clientes[i].socket != 0)
					{
						struct json_object *user_obj = json_object_new_object();
						json_object_object_add(user_obj, "nombre", json_object_new_string(clientes[i].nombre));
						json_object_object_add(user_obj, "status", json_object_new_string(clientes[i].status));
						json_object_array_add(usuarios_array, user_obj);
					}
				}
				pthread_mutex_unlock(&clientes_mutex);

				json_object_object_add(respuesta, "tipo", json_object_new_string("lista_usuarios"));
				json_object_object_add(respuesta, "usuarios", usuarios_array);
				send(cliente_socket, json_object_to_json_string(respuesta), strlen(json_object_to_json_string(respuesta)), 0);
				json_object_put(respuesta);
			}
			else if (strncmp(contenido, "/msg", 4) == 0)
			{
				char destinatario[50], mensaje[BUFFER_SIZE];
				if (sscanf(contenido, "/msg %s %[^\n]", destinatario, mensaje) == 2)
				{
					enviar_mensaje_privado(nombre, destinatario, mensaje, cliente_socket);
				}
				else
				{
					enviar_respuesta(cliente_socket, "error", "Formato incorrecto. Use: /msg usuario mensaje");
				}
			}
			else if (strncmp(contenido, "/status", 7) == 0)
			{
				char nuevo_status[10];
				if (sscanf(contenido, "/status %s", nuevo_status) == 1)
				{
					pthread_mutex_lock(&clientes_mutex);
					for (int i = 0; i < MAX_CLIENTES; i++)
					{
						if (clientes[i].socket == cliente_socket)
						{
							strncpy(clientes[i].status, nuevo_status, sizeof(clientes[i].status) - 1);
							break;
						}
					}
					pthread_mutex_unlock(&clientes_mutex);
					enviar_respuesta(cliente_socket, "sistema", "Estado actualizado");
				}
			}
		}
		else
		{
			// Mensaje normal
			struct json_object *mensaje_json = json_object_new_object();
			json_object_object_add(mensaje_json, "usuario", json_object_new_string(nombre));
			json_object_object_add(mensaje_json, "tipo", json_object_new_string("mensaje"));
			json_object_object_add(mensaje_json, "contenido", json_object_new_string(contenido));
			broadcast(json_object_to_json_string(mensaje_json), cliente_socket);
			json_object_put(mensaje_json);
		}

		json_object_put(parsed_json);
	}

	// Manejar desconexión
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

	// Notificar desconexión
	struct json_object *desconexion_json = json_object_new_object();
	json_object_object_add(desconexion_json, "tipo", json_object_new_string("sistema"));

	char temp_msg2[100];
	snprintf(temp_msg2, sizeof(temp_msg2), "%s se ha desconectado", nombre);
	json_object_object_add(desconexion_json, "contenido", json_object_new_string(temp_msg2));

	broadcast(json_object_to_json_string(desconexion_json), cliente_socket);
	json_object_put(desconexion_json);

#ifdef _WIN32
	closesocket(cliente_socket);
#else
	close(cliente_socket);
#endif
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Uso: %s <puerto>\n", argv[0]);
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

	int servidor_socket, cliente_socket;
	struct sockaddr_in servidor_addr, cliente_addr;
	socklen_t cliente_len = sizeof(cliente_addr);

	servidor_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (servidor_socket < 0)
	{
		perror("Error al crear el socket del servidor");
#ifdef _WIN32
		WSACleanup();
#endif
		return 1;
	}

	// Configuración del socket para reutilizar dirección
	int opt = 1;
#ifdef _WIN32
	setsockopt(servidor_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
	setsockopt(servidor_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

	servidor_addr.sin_family = AF_INET;
	servidor_addr.sin_addr.s_addr = INADDR_ANY;
	servidor_addr.sin_port = htons(atoi(argv[1]));

	if (bind(servidor_socket, (struct sockaddr *)&servidor_addr, sizeof(servidor_addr)) < 0)
	{
		perror("Error al hacer bind");
#ifdef _WIN32
		closesocket(servidor_socket);
		WSACleanup();
#else
		close(servidor_socket);
#endif
		return 1;
	}

	if (listen(servidor_socket, MAX_CLIENTES) < 0)
	{
		perror("Error al escuchar conexiones");
#ifdef _WIN32
		closesocket(servidor_socket);
		WSACleanup();
#else
		close(servidor_socket);
#endif
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
#ifdef _WIN32
			closesocket(cliente_socket);
#else
			close(cliente_socket);
#endif
		}

		pthread_detach(hilo); // Para que los recursos se liberen automáticamente
	}

#ifdef _WIN32
	closesocket(servidor_socket);
	WSACleanup();
#else
	close(servidor_socket);
#endif

	return 0;
}