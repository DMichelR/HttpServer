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
int server_fd;

char *get_path() {
    char *path = malloc(BUFFER_SIZE);

    if (getcwd(path, BUFFER_SIZE) == NULL) {
        perror("Failed to get current working directory.");
        return NULL;
    }

    strcat(path, "/");

    return path;
}

void handle_sigint(int sig) {
    printf("\nServer stopped.\n");
    close(server_fd);
    exit(EXIT_SUCCESS);
}

void send_response(int client_socket, const char *status, const char *content_type, const char *message) {
    char response[BUFFER_SIZE];
    sprintf(response, "HTTP/1.0 %s\r\n%sContent-Type: %s\r\n\r\n%s", status, server_info, content_type, message);
    send(client_socket, response, strlen(response), 0);
}

void handle_file_request(int client_socket, const char *filename, const char *content_type) {
    char full_path[BUFFER_SIZE];
    char *path = get_path();
    sprintf(full_path, "%s%s", path, filename);
    printf("Opening file: %s\n", full_path);

    FILE *file = fopen(full_path, "r");
    if (file != NULL) {
        send_response(client_socket, "200 OK", content_type, "");
        char buffer[BUFFER_SIZE];
        while (fgets(buffer, BUFFER_SIZE, file) != NULL) {
            send(client_socket, buffer, strlen(buffer), 0);
        }
        fclose(file);
    } else {
        send_response(client_socket, "404 Not Found", "text/plain", "File not found.");
    }
}

void handle_python_request(int client_socket, const char *filename) {
    char full_path[BUFFER_SIZE];
    char *path = get_path();
    sprintf(full_path, "%s%s", path, filename);
    printf("Opening file: %s\n", full_path);

    FILE *file = fopen(full_path, "r");
    if (file != NULL) {
        FILE *python_output;
        char python_command[BUFFER_SIZE];
        char python_buffer[BUFFER_SIZE];

        sprintf(python_command, "python3 %s", full_path);
        python_output = popen(python_command, "r");
        if (python_output != NULL) {
            send_response(client_socket, "200 OK", "text/plain", "");
            while (fgets(python_buffer, BUFFER_SIZE, python_output) != NULL) {
                send(client_socket, python_buffer, strlen(python_buffer), 0);
            }
            pclose(python_output);
        } else {
            send_response(client_socket, "500 Internal Server Error", "text/py", "Error executing Python script.");
        }
        fclose(file);
    } else {
        send_response(client_socket, "404 Not Found", "text/plain", "File not found.");
    }
}

void handle_connection(int client_socket) {
    char buffer[BUFFER_SIZE];
    read(client_socket, buffer, BUFFER_SIZE);

    char method[5];
    char path[BUFFER_SIZE];
    sscanf(buffer, "%s %s", method, path);

    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/") == 0) {
            send_response(client_socket, "200 OK", "text/plain", server_info);
        } else {
            char *filename = strrchr(path, '/') + 1;
            handle_file_request(client_socket, filename, "text/html");
        }
    } else if (strcmp(method, "POST") == 0) {
        if (strstr(path, ".py") != NULL) {
            char *filename = strrchr(path, '/') + 1;
            handle_python_request(client_socket, filename);
        } else {
            send_response(client_socket, "400 Bad Request", "text/plain", "Bad Request.");
        }
    } else {
        send_response(client_socket, "400 Bad Request", "text/plain", "Bad Request.");
    }

    close(client_socket);
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address,
             sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, handle_sigint);

    while (1) {
        printf("\nWaiting...\n\n");
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address,
                                    (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        pid_t pid = fork();
        if (pid == 0) {
            close(server_fd);
            handle_connection(client_socket);
            exit(EXIT_SUCCESS);
        } else if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else {
            close(client_socket);
        }
    }
    return 0;
}
