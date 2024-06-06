#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#define PORT 8889
#define BUF_SIZE 1024

int client_sock = -1; // Socket para la conexión del cliente

void *handle_client(void *arg);
void execute_command(char *command, int sock);
void *get_user_input(void *arg);
void connect_to_server(char *ip_address);

// Declaraciones de funciones específicas para cada comando
void open_connection(char *ip_address);
void close_connection();
void quit_program();
void change_directory(char *directory, int sock);
void get_file(char *filename);
void change_local_directory(char *directory);
void list_files();
void put_file(char *filename);
void print_working_directory();

int main(int argc, char *argv[]) {
    int server_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size;
    pthread_t t_id;

    // Create server socket
    server_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("socket() error");
        exit(1);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    // Bind server socket
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind() error");
        close(server_sock);
        exit(1);
    }

    // Listen for connections
    if (listen(server_sock, 5) == -1) {
        perror("listen() error");
        close(server_sock);
        exit(1);
    }

    // Thread to handle user input
    pthread_create(&t_id, NULL, get_user_input, NULL);
    pthread_detach(t_id);

    // Accept client connections
    while (1) {
        client_addr_size = sizeof(client_addr);
        int client_sock_temp = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_size);
        if (client_sock_temp == -1) {
            perror("accept() error");
            continue;
        }

        int *client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = client_sock_temp;
        pthread_create(&t_id, NULL, handle_client, (void*)client_sock_ptr);
        pthread_detach(t_id);
    }

    close(server_sock);
    return 0;
}

void *handle_client(void *arg) {
    int client_sock = *((int*)arg);
    free(arg);
    char command[BUF_SIZE];

    while (1) {
        int str_len = read(client_sock, command, sizeof(command) - 1);
        if (str_len <= 0) {
            close(client_sock);
            return NULL;
        }

        command[str_len] = 0;
        execute_command(command, client_sock);
    }
}

void *get_user_input(void *arg) {
    char command[BUF_SIZE];

    while (1) {
        printf("bftp> ");
        fgets(command, BUF_SIZE, stdin);
        execute_command(command, client_sock);
    }
    return NULL;
}

void execute_command(char *command, int sock) {
    // Remove trailing newline character
    command[strcspn(command, "\n")] = 0;

    // Split command into parts
    char *token = strtok(command, " ");
    if (token == NULL) {
        return;
    }

    if (strcmp(token, "open") == 0) {
        char *ip_address = strtok(NULL, " ");
        if (ip_address) {
            open_connection(ip_address);
        }
    } else if (strcmp(token, "close") == 0) {
        close_connection();
    } else if (strcmp(token, "quit") == 0) {
        quit_program();
    } else if (strcmp(token, "cd") == 0) {
        char *directory = strtok(NULL, " ");
        if (directory) {
            change_directory(directory, sock);
        }
    } else if (strcmp(token, "get") == 0) {
        char *filename = strtok(NULL, " ");
        if (filename) {
            get_file(filename);
        }
    } else if (strcmp(token, "lcd") == 0) {
        char *directory = strtok(NULL, " ");
        if (directory) {
            change_local_directory(directory);
        }
    } else if (strcmp(token, "ls") == 0) {
        list_files(sock);
    } else if (strcmp(token, "put") == 0) {
        char *filename = strtok(NULL, " ");
        if (filename) {
            put_file(filename);
        }
    } else if (strcmp(token, "pwd") == 0) {
        if (sock == -1) {
            print_working_directory();
        } else {
            char command[BUF_SIZE] = "pwd";
            write(sock, command, strlen(command));

            // Recibir respuesta del servidor
            char response[BUF_SIZE];
            int str_len = read(sock, response, sizeof(response) - 1);
            if (str_len > 0) {
                response[str_len] = 0;
                printf("%s\n", response);
            }
        }
    } else {
        printf("Unknown command: %s\n", token);
    }
}


// Implementaciones de las funciones específicas para cada comando
void open_connection(char *ip_address) {
    if (client_sock != -1) {
        printf("Ya hay una conexión abierta. Primero cierre la conexión actual.\n");
        return;
    }

    connect_to_server(ip_address);
}

void connect_to_server(char *ip_address) {
    struct sockaddr_in server_addr;

    client_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("socket() error");
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip_address);
    server_addr.sin_port = htons(PORT);

    if (connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect() error");
        close(client_sock);
        client_sock = -1;
        return;
    }

    printf("Conectado a %s\n", ip_address);
}

void close_connection() {
    if (client_sock == -1) {
        printf("No hay ninguna conexión abierta.\n");
        return;
    }

    close(client_sock);
    client_sock = -1;
    printf("Conexión cerrada.\n");
}

void quit_program() {
    if (client_sock != -1) {
        close(client_sock);
    }
    printf("Terminando el programa.\n");
    exit(0);
}

void change_directory(char *directory, int sock) {
    if (sock == -1) {
        if (chdir(directory) == 0) {
            printf("Directorio cambiado a %s\n", directory);
        } else {
            perror("chdir() error");
        }
    } else {
        char command[BUF_SIZE];
        snprintf(command, sizeof(command), "cd %s", directory);
        write(sock, command, strlen(command));

        // Recibir respuesta del servidor
        char response[BUF_SIZE];
        int str_len = read(sock, response, sizeof(response) - 1);
        if (str_len > 0) {
            response[str_len] = 0;
            printf("%s\n", response);
        }
    }
}

void get_file(char *filename) {
    if (client_sock == -1) {
        printf("No hay ninguna conexión abierta.\n");
        return;
    }

    char command[BUF_SIZE];
    snprintf(command, sizeof(command), "get %s", filename);
    write(client_sock, command, strlen(command));

    // Aquí debe implementarse la lógica para recibir el archivo desde el servidor
}

void change_local_directory(char *directory) {
    if (chdir(directory) == -1) {
        perror("chdir() error");
    }
}

void list_files(int sock) {
    if (sock == -1) {
        DIR *d;
        struct dirent *dir;
        d = opendir(".");
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                printf("%s\n", dir->d_name);
            }
            closedir(d);
        } else {
            perror("opendir() error");
        }
    } else {
        char command[BUF_SIZE] = "ls";
        write(sock, command, strlen(command));

        // Recibir respuesta del servidor
        char response[BUF_SIZE];
        int str_len = read(sock, response, sizeof(response) - 1);
        if (str_len > 0) {
            response[str_len] = 0;
            printf("%s\n", response);
        }
    }
}

void put_file(char *filename) {
    if (client_sock == -1) {
        printf("No hay ninguna conexión abierta.\n");
        return;
    }

    char command[BUF_SIZE];
    snprintf(command, sizeof(command), "put %s", filename);
    write(client_sock, command, strlen(command));

    // Aquí debe implementarse la lógica para enviar el archivo al servidor
}

void print_working_directory() {
    char cwd[BUF_SIZE];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("getcwd() error");
    }
}

//gcc bftp.c -o bftp -pthread
//./bftp
//open 192.168.0.15