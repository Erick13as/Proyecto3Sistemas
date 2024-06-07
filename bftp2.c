#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 8889
#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024

int server_sockfd;

// Función para manejar la lógica de "open"
int handle_open(const char *ip_address) {
    printf("Connecting to %s...\n", ip_address);

    // Create socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    // Set server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip_address);
    server_addr.sin_port = htons(PORT);

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        return -1;
    }

    printf("Connected to %s:%d\n", ip_address, PORT);

    return sockfd;
}

void *handle_client(void *arg) {
    int client_sockfd = *((int *)arg);
    char buffer[BUFFER_SIZE];

    // Handle client commands
    while (1) {
        // Read client command
        memset(buffer, 0, BUFFER_SIZE);
        read(client_sockfd, buffer, BUFFER_SIZE);

        // Process command
        if (strcmp(buffer, "quit\n") == 0 || strcmp(buffer, "quit\r\n") == 0) {
            break;  // Exit thread
        } else if (strncmp(buffer, "open ", 5) == 0) {
            char *ip_address = buffer + 5; // Skip "open "
            ip_address[strlen(ip_address) - 1] = '\0'; // Remove newline character
            int remote_sockfd = handle_open(ip_address);
            if (remote_sockfd >= 0) {
                // Forward communication between client and remote server
                while (1) {
                    ssize_t bytes_received = recv(remote_sockfd, buffer, BUFFER_SIZE, 0);
                    if (bytes_received <= 0) {
                        perror("Error receiving data from server");
                        close(remote_sockfd);
                        close(client_sockfd);
                        return NULL;
                    }
                    send(client_sockfd, buffer, bytes_received, 0);
                }
            } else {
                write(client_sockfd, "Connection failed\n", strlen("Connection failed\n"));
            }
        } else if (strcmp(buffer, "close\n") == 0 || strcmp(buffer, "close\r\n") == 0) {
            break;  // Close connection
        } else {
            // Handle other commands
            // Example: echo command
            write(client_sockfd, buffer, strlen(buffer));
        }
    }

    // Close client socket
    close(client_sockfd);
    return NULL;
}

int main() {
    // Server socket
    struct sockaddr_in server_addr;
    int client_sockfd;
    pthread_t client_thread;

    // Create server socket
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        perror("Server socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind server socket
    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_sockfd, MAX_CLIENTS) < 0) {
        perror("Listening failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    // Accept connections and create client threads
    while (1) {
        // Accept client connection
        client_sockfd = accept(server_sockfd, NULL, NULL);
        if (client_sockfd < 0) {
            perror("Acceptance failed");
            exit(EXIT_FAILURE);
        }

        // Create thread to handle client
        if (pthread_create(&client_thread, NULL, handle_client, &client_sockfd) != 0) {
            perror("Client thread creation failed");
            exit(EXIT_FAILURE);
        }
    }

    // Close server socket
    close(server_sockfd);

    return 0;
}
