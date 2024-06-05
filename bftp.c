#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8889
#define BUF_SIZE 1024

void *handle_client(void *arg);
void execute_command(char *command);

// Declaraciones de funciones específicas para cada comando
void open_connection(char *ip_address);
void close_connection();
void quit_program();
void change_directory(char *directory);
void get_file(char *filename);
void change_local_directory(char *directory);
void list_files();
void put_file(char *filename);
void print_working_directory();

int main(int argc, char *argv[]) {
    int server_sock, client_sock;
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

    while (1) {
        client_addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_size);
        if (client_sock == -1) {
            perror("accept() error");
            continue;
        }

        pthread_create(&t_id, NULL, handle_client, (void*)&client_sock);
        pthread_detach(t_id);
    }

    close(server_sock);
    return 0;
}

void *handle_client(void *arg) {
    int client_sock = *((int*)arg);
    char command[BUF_SIZE];

    while (1) {
        int str_len = read(client_sock, command, sizeof(command) - 1);
        if (str_len <= 0) {
            close(client_sock);
            return NULL;
        }

        command[str_len] = 0;
        execute_command(command);
    }
}

void execute_command(char *command) {
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
            change_directory(directory);
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
        list_files();
    } else if (strcmp(token, "put") == 0) {
        char *filename = strtok(NULL, " ");
        if (filename) {
            put_file(filename);
        }
    } else if (strcmp(token, "pwd") == 0) {
        print_working_directory();
    } else {
        printf("Unknown command: %s\n", token);
    }
}

// Implementaciones de las funciones específicas para cada comando
void open_connection(char *ip_address) {
    printf("open command with ip address: %s\n", ip_address);
    // Implementar la lógica de conexión
}

void close_connection() {
    printf("close command\n");
    // Implementar la lógica de cerrar la conexión
}

void quit_program() {
    printf("quit command\n");
    // Implementar la lógica de terminar el programa
    exit(0);
}

void change_directory(char *directory) {
    printf("cd command with directory: %s\n", directory);
    // Implementar la lógica de cambiar de directorio
}

void get_file(char *filename) {
    printf("get command with filename: %s\n", filename);
    // Implementar la lógica de obtener un archivo
}

void change_local_directory(char *directory) {
    printf("lcd command with directory: %s\n", directory);
    // Implementar la lógica de cambiar de directorio local
}

void list_files() {
    printf("ls command\n");
    // Implementar la lógica de listar archivos
}

void put_file(char *filename) {
    printf("put command with filename: %s\n", filename);
    // Implementar la lógica de enviar un archivo
}

void print_working_directory() {
    printf("pwd command\n");
    // Implementar la lógica de mostrar el directorio activo
}

//gcc bftp.c -o bftp -pthread
//./bftp
//open 192.168.0.20