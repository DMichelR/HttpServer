#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CONNECTIONS 30

const char *server_info = "Server: MyHTTPServer/1.0\r\n\r\n";
const char *BASE_PATH = "/home/fundacion/CLionProjects/Unix-Final/";


void handle_connection(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    char response[BUFFER_SIZE] = {0};

    // Read the HTTP request from the client
    read(client_socket, buffer, BUFFER_SIZE);

    // Parse the HTTP request to check for GET or POST and the requested path
    char method[5];
    char path[BUFFER_SIZE];
    sscanf(buffer, "%s %s", method, path);

    if  (strcmp(method, "GET") == 0) {
        // Extract the filename from the path
        char *filename;
        if (strcmp(path, "/") == 0) {
            // Send the server info as response for "/"
            sprintf(response, "HTTP/1.0 200 OK\r\n%sContent-Type: text/plain\r\n%s", server_info, server_info);
            send(client_socket, response, strlen(response), 0);
            return;
        } else {
            filename = strrchr(path, '/') + 1;

            // Open the requested file
            char full_path[BUFFER_SIZE];
            sprintf(full_path, "%s%s", BASE_PATH, filename);
            printf("Opening file: %s\n", full_path);

            FILE *file = fopen(full_path, "r");
            if (file != NULL) {
                // File found, send its content in the response
                char header[BUFFER_SIZE];
                sprintf(header, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
                send(client_socket, header, strlen(header), 0);
                while (fgets(buffer, BUFFER_SIZE, file) != NULL) {
                    send(client_socket, buffer, strlen(buffer), 0);
                }
                fclose(file);
            } else {
                // File not found
                sprintf(response, "HTTP/1.0 404 Not Found\r\nContent-Type: text/plain\r\n\r\nFile not found.");
                send(client_socket, response, strlen(response), 0);
            }
        }
    } else if (strcmp(method, "POST") == 0 && strstr(path, ".py") != NULL) {
        // Handle POST request to a specific Python file
        // Extract the filename from the path
        char *filename = strrchr(path, '/') + 1;

        char full_path[BUFFER_SIZE];
        sprintf(full_path, "%s%s", BASE_PATH, filename);
        printf("Opening file: %s\n", full_path);
        // Open the requested Python file
        FILE *file = fopen(full_path, "r");

        if (file != NULL) {
            // File found, execute it with Python interpreter
            FILE *python_output;
            char python_command[BUFFER_SIZE];
            char python_buffer[BUFFER_SIZE];
            char response[BUFFER_SIZE];

            // Build the command to execute the Python script
            sprintf(python_command, "python3 %s%s", BASE_PATH, filename);

            // Open a pipe to read the output of the Python script
            python_output = popen(python_command, "r");
            if (python_output != NULL) {
                // Send HTTP response header
                sprintf(response, "HTTP/1.0 200 OK\r\n%sContent-Type: text/plain\r\n\r\n", server_info);
                send(client_socket, response, strlen(response), 0);

                // Read and send the output of the Python script line by line
                while (fgets(python_buffer, BUFFER_SIZE, python_output) != NULL) {
                    send(client_socket, python_buffer, strlen(python_buffer), 0);
                }

                // Close the pipe
                pclose(python_output);
            } else {
                // Error executing the Python script
                sprintf(response, "HTTP/1.0 500 Internal Server Error\r\n%sContent-Type: text/plain\r\n\r\nError executing Python script.", server_info);
                send(client_socket, response, strlen(response), 0);
            }

            // Close the file
            fclose(file);
        } else {
            // File not found, return 404 Not Found
            sprintf(response, "HTTP/1.0 404 Not Found\r\n%sContent-Type: text/plain\r\n\r\nFile not found.", server_info);
            send(client_socket, response, strlen(response), 0);
        }
    }
    else {
        // Invalid request method or path
        sprintf(response, "HTTP/1.0 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nBad Request.");
        send(client_socket, response, strlen(response), 0);
    }

    close(client_socket);
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address,
             sizeof(address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (1) {
        printf("\nWaiting for a connection...\n\n");
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address,
                                    (socklen_t*)&addrlen))<0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Fork a new process to handle the connection
        pid_t pid = fork();
        if (pid == 0) { // Child process
            close(server_fd); // Close the listening socket in the child process
            handle_connection(client_socket);
            exit(EXIT_SUCCESS); // Exit the child process after handling the connection
        } else if (pid < 0) { // Fork failed
            perror("fork");
            exit(EXIT_FAILURE);
        } else { // Parent process
            close(client_socket); // Close the client socket in the parent process
        }
    }
    return 0;
}
