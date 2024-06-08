#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8889
#define BUFFER_SIZE 1024

typedef struct {
    int sock;
    struct sockaddr_in address;
    char current_directory[BUFFER_SIZE];
} connection_t;

int client_socket = -1;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_connection(void *ptr);
void *handle_commands(void *ptr);
void execute_remote_command(const char *command, char *response, size_t size, connection_t *connection);
void list_local_files(char *response, size_t size);
void handle_file_transfer(int client_socket, const char *command, connection_t *connection);
void send_error_message(int socket, const char *message);

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
            getcwd(connection->current_directory, sizeof(connection->current_directory));

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

        if (strncmp(buffer, "get ", 4) == 0 || strncmp(buffer, "put ", 4) == 0) {
            handle_file_transfer(connection->sock, buffer, connection);
        } else {
            char response[BUFFER_SIZE] = {0};
            execute_remote_command(buffer, response, sizeof(response), connection);
            send(connection->sock, response, strlen(response), 0);
        }
    }

    close(connection->sock);
    free(connection);
    return NULL;
}

void execute_remote_command(const char *command, char *response, size_t size, connection_t *connection) {
    FILE *fp;
    char path[BUFFER_SIZE];
    char cmd[BUFFER_SIZE];
    char temp_directory[BUFFER_SIZE];

    if (getcwd(temp_directory, sizeof(temp_directory)) == NULL) {
        snprintf(response, size, "Error: %s\n", strerror(errno));
        return;
    }

    if (chdir(connection->current_directory) != 0) {
        snprintf(response, size, "Error: %s\n", strerror(errno));
        return;
    }

    if (strncmp(command, "cd ", 3) == 0) {
        char *dir = command + 3;
        if (chdir(dir) == 0) {
            getcwd(connection->current_directory, sizeof(connection->current_directory));
            snprintf(response, size, "Changed directory to %s\n", connection->current_directory);
        } else {
            snprintf(response, size, "Error: %s\n", strerror(errno));
        }
    } else if (strcmp(command, "ls") == 0) {
        snprintf(cmd, sizeof(cmd), "ls");
        fp = popen(cmd, "r");
        if (fp == NULL) {
            snprintf(response, size, "Error: %s\n", strerror(errno));
            return;
        }
        while (fgets(path, sizeof(path) - 1, fp) != NULL) {
            strncat(response, path, size - strlen(response) - 1);
        }
        pclose(fp);
    } else {
        fp = popen(command, "r");
        if (fp == NULL) {
            snprintf(response, size, "Error: %s\n", strerror(errno));
            return;
        }
        while (fgets(path, sizeof(path) - 1, fp) != NULL) {
            strncat(response, path, size - strlen(response) - 1);
        }
        pclose(fp);
    }

    chdir(temp_directory);
}

void list_local_files(char *response, size_t size) {
    DIR *dir;
    struct dirent *ent;

    dir = opendir(".");
    if (dir == NULL) {
        snprintf(response, size, "Error: %s\n", strerror(errno));
        return;
    }

    while ((ent = readdir(dir)) != NULL) {
        strncat(response, ent->d_name, size - strlen(response) - 1);
        strncat(response, "\n", size - strlen(response) - 1);
    }
    closedir(dir);
}

void handle_file_transfer(int client_socket, const char *command, connection_t *connection) {
    char buffer[BUFFER_SIZE];
    int file;
    ssize_t bytes;

    char temp_directory[BUFFER_SIZE];
    if (getcwd(temp_directory, sizeof(temp_directory)) == NULL) {
        send_error_message(client_socket, strerror(errno));
        return;
    }

    if (chdir(connection->current_directory) != 0) {
        send_error_message(client_socket, strerror(errno));
        return;
    }

    if (strncmp(command, "get ", 4) == 0) {
        const char *filename = command + 4;
        file = open(filename, O_RDONLY);
        if (file < 0) {
            send_error_message(client_socket, strerror(errno));
            return;
        }

        struct stat file_stat;
        if (fstat(file, &file_stat) < 0) {
            send_error_message(client_socket, strerror(errno));
            close(file);
            return;
        }

        off_t file_size = file_stat.st_size;
        send(client_socket, &file_size, sizeof(file_size), 0);

        while ((bytes = read(file, buffer, sizeof(buffer))) > 0) {
            send(client_socket, buffer, bytes, 0);
        }

        close(file);
    } else if (strncmp(command, "put ", 4) == 0) {
        const char *filename = command + 4;
        file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (file < 0) {
            send_error_message(client_socket, strerror(errno));
            return;
        }

        off_t file_size;
        recv(client_socket, &file_size, sizeof(file_size), 0);

        off_t bytes_received = 0;
        while (bytes_received < file_size) {
            bytes = recv(client_socket, buffer, sizeof(buffer), 0);
            if (bytes <= 0) {
                break;
            }
            write(file, buffer, bytes);
            bytes_received += bytes;
        }

        close(file);
    }

    chdir(temp_directory);
}

void send_error_message(int socket, const char *message) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "Error: %s\n", message);
    send(socket, buffer, strlen(buffer), 0);
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
        } else if (strncmp(command, "lcd ", 4) == 0) {
            char *dir = command + 4;
            if (chdir(dir) == 0) {
                printf("Changed local directory to %s\n", dir);
            } else {
                perror("Failed to change local directory");
            }
        } else if (strcmp(command, "lpwd") == 0) {
            char cwd[BUFFER_SIZE];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("Local directory: %s\n", cwd);
            } else {
                perror("getcwd() error");
            }
        } else if (strcmp(command, "lls") == 0) {
            char response[BUFFER_SIZE] = {0};
            list_local_files(response, sizeof(response));
            printf("%s", response);
        } else if (strncmp(command, "get ", 4) == 0) {
            pthread_mutex_lock(&mutex);
            if (client_socket == -1) {
                printf("No active connection to send command to\n");
                pthread_mutex_unlock(&mutex);
                continue;
            }

            send(client_socket, command, strlen(command), 0);

            char filename[BUFFER_SIZE];
            strcpy(filename, command + 4);

            off_t file_size;
            if (recv(client_socket, &file_size, sizeof(file_size), 0) <= 0) {
                perror("Failed to receive file size");
                pthread_mutex_unlock(&mutex);
                continue;
            }

            int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (file < 0) {
                perror("Failed to open file");
                pthread_mutex_unlock(&mutex);
                continue;
            }

            char buffer[BUFFER_SIZE];
            ssize_t bytes;
            off_t bytes_received = 0;
            while (bytes_received < file_size) {
                bytes = recv(client_socket, buffer, sizeof(buffer), 0);
                if (bytes <= 0) {
                    break;
                }
                write(file, buffer, bytes);
                bytes_received += bytes;
            }

            close(file);
            pthread_mutex_unlock(&mutex);
        } else if (strncmp(command, "put ", 4) == 0) {
            pthread_mutex_lock(&mutex);
            if (client_socket == -1) {
                printf("No active connection to send command to\n");
                pthread_mutex_unlock(&mutex);
                continue;
            }

            char filename[BUFFER_SIZE];
            strcpy(filename, command + 4);
            int file = open(filename, O_RDONLY);
            if (file < 0) {
                perror("Failed to open file");
                pthread_mutex_unlock(&mutex);
                continue;
            }

            struct stat file_stat;
            if (fstat(file, &file_stat) < 0) {
                perror("Failed to get file stat");
                close(file);
                pthread_mutex_unlock(&mutex);
                continue;
            }

            off_t file_size = file_stat.st_size;
            send(client_socket, command, strlen(command), 0);

            send(client_socket, &file_size, sizeof(file_size), 0);

            char buffer[BUFFER_SIZE];
            ssize_t bytes;
            while ((bytes = read(file, buffer, sizeof(buffer))) > 0) {
                send(client_socket, buffer, bytes, 0);
            }

            close(file);
            pthread_mutex_unlock(&mutex);
        } else {
            pthread_mutex_lock(&mutex);
            if (client_socket != -1) {
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
            pthread_mutex_unlock(&mutex);
        }
    }

    return NULL;
}
