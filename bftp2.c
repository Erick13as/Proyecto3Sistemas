#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>

#define PORT 8889
#define BUFFER_SIZE 1024

typedef struct {
    int sock;
    struct sockaddr_in address;
} connection_t;

int client_socket = -1;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_connection(void *ptr);
void *handle_commands(void *ptr);
void execute_remote_command(const char *command, char *response, size_t size);

int main() {
    int server_socket;
    struct sockaddr_in server_address;
    pthread_t thread;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Could not create socket");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    pthread_create(&thread, NULL, handle_commands, NULL);

    while (1) {
        connection_t *connection = malloc(sizeof(connection_t));
        socklen_t address_len = sizeof(connection->address);

        connection->sock = accept(server_socket, (struct sockaddr *)&connection->address, &address_len);
        if (connection->sock <= 0) {
            free(connection);
        } else {
            pthread_t conn_thread;
            pthread_create(&conn_thread, NULL, handle_connection, (void *)connection);
            pthread_detach(conn_thread);
        }
    }

    close(server_socket);
    return 0;
}

void *handle_connection(void *ptr) {
    connection_t *connection = (connection_t *)ptr;
    char buffer[BUFFER_SIZE];

    while (1) {
        int read_size = recv(connection->sock, buffer, sizeof(buffer), 0);
        if (read_size <= 0) {
            break;
        }
        buffer[read_size] = '\0';

        char response[BUFFER_SIZE];
        execute_remote_command(buffer, response, sizeof(response));

        send(connection->sock, response, strlen(response), 0);
    }

    close(connection->sock);
    free(connection);
    return NULL;
}

void execute_remote_command(const char *command, char *response, size_t size) {
    FILE *fp;
    char path[BUFFER_SIZE];

    fp = popen(command, "r");
    if (fp == NULL) {
        snprintf(response, size, "Failed to execute command\n");
        return;
    }

    while (fgets(path, sizeof(path) - 1, fp) != NULL) {
        strncat(response, path, size - strlen(response) - 1);
    }
    pclose(fp);
}

void *handle_commands(void *ptr) {
    char command[BUFFER_SIZE];

    while (1) {
        printf("> ");
        fflush(stdout);
        if (fgets(command, sizeof(command), stdin) == NULL) {
            continue;
        }

        command[strcspn(command, "\n")] = '\0';

        if (strncmp(command, "open", 4) == 0) {
            char *ip = strtok(command + 5, " ");
            if (ip) {
                struct sockaddr_in server_addr;

                pthread_mutex_lock(&mutex);
                if (client_socket != -1) {
                    printf("Already connected to a remote server. Please close the connection first.\n");
                    pthread_mutex_unlock(&mutex);
                    continue;
                }

                client_socket = socket(AF_INET, SOCK_STREAM, 0);
                if (client_socket < 0) {
                    perror("Socket creation failed");
                    pthread_mutex_unlock(&mutex);
                    continue;
                }

                server_addr.sin_family = AF_INET;
                server_addr.sin_port = htons(PORT);

                if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
                    perror("Invalid address");
                    close(client_socket);
                    client_socket = -1;
                    pthread_mutex_unlock(&mutex);
                    continue;
                }

                if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                    perror("Connection failed");
                    close(client_socket);
                    client_socket = -1;
                    pthread_mutex_unlock(&mutex);
                    continue;
                }

                printf("Connected to %s\n", ip);
                pthread_mutex_unlock(&mutex);
            }
        } else if (strcmp(command, "close") == 0) {
            pthread_mutex_lock(&mutex);
            if (client_socket != -1) {
                close(client_socket);
                client_socket = -1;
                printf("Connection closed\n");
            } else {
                printf("No active connection to close\n");
            }
            pthread_mutex_unlock(&mutex);
        } else if (strcmp(command, "quit") == 0) {
            pthread_mutex_lock(&mutex);
            if (client_socket != -1) {
                close(client_socket);
            }
            printf("Exiting...\n");
            pthread_mutex_unlock(&mutex);
            exit(EXIT_SUCCESS);
        } else if (client_socket != -1) {
            send(client_socket, command, strlen(command), 0);
            char response[BUFFER_SIZE] = {0};
            int read_size = recv(client_socket, response, sizeof(response) - 1, 0);
            if (read_size > 0) {
                response[read_size] = '\0';
                printf("%s", response);
            }
        } else {
            printf("No active connection to send command to\n");
        }
    }

    return NULL;
}