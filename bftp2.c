#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 8889
#define MAX_CLIENTS 5
#define MAX_COMMAND_SIZE 100

// Estructura para los datos de cada cliente
typedef struct {
    int socket;
    char current_directory[100];
} ClientData;

// Función para manejar la conexión con un cliente
void *handle_client(void *arg) {
    ClientData *client = (ClientData *)arg;
    char command[MAX_COMMAND_SIZE];

    while (1) {
        printf("btfp> ");
        fgets(command, MAX_COMMAND_SIZE, stdin);
        command[strlen(command) - 1] = '\0'; // Eliminar el carácter de nueva línea

        if (strcmp(command, "quit") == 0) {
            close(client->socket);
            pthread_exit(NULL);
        } else if (strcmp(command, "pwd") == 0) {
            // Enviar comando pwd al cliente remoto
            // (tendrías que implementar esta función)
        } else if (strncmp(command, "open ", 5) == 0) {
            // Establecer conexión con el servidor remoto
            char *ip_address = strtok(command + 5, " ");
            int port = atoi(strtok(NULL, " "));
            int remote_socket;
            struct sockaddr_in server_address;

            // Crear socket para la conexión remota
            remote_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (remote_socket == -1) {
                perror("Error al crear el socket remoto");
                continue;
            }

            // Configurar la estructura sockaddr_in
            server_address.sin_family = AF_INET;
            server_address.sin_addr.s_addr = inet_addr(ip_address);
            server_address.sin_port = htons(port);

            // Conectar al servidor remoto
            if (connect(remote_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
                perror("Error al conectar al servidor remoto");
                close(remote_socket);
                continue;
            }

            printf("Conectado al servidor remoto\n");
            client->socket = remote_socket;
        } else if (strcmp(command, "close") == 0) {
            // Cerrar la conexión con el servidor remoto
            close(client->socket);
            printf("Conexión cerrada\n");
        } else if (strncmp(command, "cd ", 3) == 0) {
            // Cambiar de directorio remoto
            // (tendrías que implementar esta función)
        } else {
            printf("Comando no reconocido\n");
        }
    }
}

int main() {
    int server_socket, client_socket, c;
    struct sockaddr_in server, client;
    pthread_t thread_id;
    ClientData clients[MAX_CLIENTS];
    int num_clients = 0;

    // Crear socket servidor
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    // Configurar la estructura sockaddr_in
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    // Enlazar el socket a la dirección y puerto especificados
    if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Error al enlazar");
        exit(EXIT_FAILURE);
    }

    // Escuchar por conexiones entrantes
    listen(server_socket, MAX_CLIENTS);

    // Aceptar conexiones entrantes
    c = sizeof(struct sockaddr_in);
    while ((client_socket = accept(server_socket, (struct sockaddr *)&client, (socklen_t *)&c))) {
        printf("Nueva conexión aceptada\n");

        // Verificar si hay espacio para otro cliente
        if (num_clients >= MAX_CLIENTS) {
            printf("No hay espacio para más clientes\n");
            close(client_socket);
            continue;
        }

        // Agregar el nuevo cliente a la lista
        clients[num_clients].socket = client_socket;
        strcpy(clients[num_clients].current_directory, "/"); // Directorio inicial
        num_clients++;

        // Crear un hilo para manejar la conexión con el cliente
        if (pthread_create(&thread_id, NULL, handle_client, (void *)&clients[num_clients - 1]) < 0) {
            perror("Error al crear el hilo");
            exit(EXIT_FAILURE);
        }
    }

    if (client_socket < 0) {
        perror("Error al aceptar la conexión");
        exit(EXIT_FAILURE);
    }

    return 0;
}
