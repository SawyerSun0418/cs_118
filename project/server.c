#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <dirent.h>
#include <stdbool.h>

#define PORT 8080
#define HTTP_VERSION "HTTP/1.0"
#define CONTENT_TYPE_HTML "text/html"
#define CONTENT_TYPE_TXT "text/plain"
#define CONTENT_TYPE_JPEG "image/jpeg"
#define CONTENT_TYPE_PNG "image/png"



typedef struct {
    char key[1024];
    char value[1024];
} KeyValuePair;

KeyValuePair *file_map;
size_t file_map_size = 0;

void build_file_map() {
    DIR *dir = opendir(".");
    if (dir == NULL) {
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }

    size_t capacity = 10;
    file_map = malloc(capacity * sizeof(KeyValuePair));

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (file_map_size == capacity) {
            capacity *= 2;
            file_map = realloc(file_map, capacity * sizeof(KeyValuePair));
        }

        strncpy(file_map[file_map_size].value, ent->d_name, sizeof(file_map[file_map_size].value));

        for (size_t i = 0; i < strlen(ent->d_name); ++i) {
            file_map[file_map_size].key[i] = tolower(ent->d_name[i]);
        }
        file_map[file_map_size].key[strlen(ent->d_name)] = '\0';

        file_map_size++;
    }

    closedir(dir);
}

void send_404(int client_socket) {
    const char *response_header = HTTP_VERSION " 404 Not Found\r\n"
                                   "Content-Type: text/html\r\n"
                                   "Connection: close\r\n"
                                   "\r\n";
    const char *response_body = "<!DOCTYPE html>"
                                "<html><head>"
                                "<title>404 Not Found</title>"
                                "</head><body>"
                                "<h1>404 Not Found</h1>"
                                "<p>The requested file was not found on this server.</p>"
                                "</body></html>";

    send(client_socket, response_header, strlen(response_header), 0);
    send(client_socket, response_body, strlen(response_body), 0);
}






void send_file(FILE *file, int client_socket, const char *content_type) {
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char response_header[2048];
    snprintf(response_header, sizeof(response_header),
             HTTP_VERSION " 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "\r\n",
             content_type, file_size);

    send(client_socket, response_header, strlen(response_header), 0);
    const size_t buffer_size = 4096;
    char buffer[buffer_size];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, buffer_size, file)) > 0) {
        ssize_t bytes_sent = 0;
        while (bytes_sent < bytes_read) {
            ssize_t result = send(client_socket, buffer + bytes_sent, bytes_read - bytes_sent, 0);
            if (result == -1) {
                perror("Error sending file");
                return;
            }
            bytes_sent += result;
        }
    }
}



void process_request(const char *request, int client_socket) {
    char method[16], path[1024], protocol[16];
    sscanf(request, "%s %s %s", method, path, protocol);
    if (strcmp(method, "GET") != 0) {
        send_404(client_socket);
        return;
    }
    if (path[0] == '/') {
        memmove(path, path + 1, strlen(path));
    }
    char path_lower[strlen(path) + 1];
    for (int i = 0; path[i]; ++i) {
        path_lower[i] = tolower(path[i]);
    }
    path_lower[strlen(path)] = '\0';
    DIR *dir = opendir(".");
    if (dir == NULL) {
        send_404(client_socket);
        return;
    }
    char *file_name = NULL;
    for (size_t i = 0; i < file_map_size; ++i) {
        if (strcmp(file_map[i].key, path_lower) == 0) {
            file_name = file_map[i].value;
            break;
        }
    }
    closedir(dir);
    if (file_name == NULL) {
        send_404(client_socket);
        return;
    }
    const char *content_type;
    char *file_ext = strrchr(file_name, '.');
    if (file_ext && (strcmp(file_ext, ".html") == 0 || strcmp(file_ext, ".htm") == 0)) {
        content_type = CONTENT_TYPE_HTML;
    } else if (file_ext && strcmp(file_ext, ".txt") == 0) {
        content_type = CONTENT_TYPE_TXT;
    } else if (file_ext && (strcmp(file_ext, ".jpg") == 0 || strcmp(file_ext, ".jpeg") == 0)) {
        content_type = CONTENT_TYPE_JPEG;
    } else if (file_ext && strcmp(file_ext, ".png") == 0) {
        content_type = CONTENT_TYPE_PNG;
    } else {
        send_404(client_socket);
        return;
    }
    FILE *file = fopen(file_name, "rb");
    if (file == NULL) {
        send_404(client_socket);
        return;
    }
    send_file(file, client_socket, content_type);
    fclose(file);
}







int main() {
    build_file_map();
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    if (listen(server_socket, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    //printf("Server running on port %d\n", PORT);
    while (1) {
        int client_socket;
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("accept");
            continue;
        }
        char request[4096];
        ssize_t bytes_received = recv(client_socket, request, sizeof(request) - 1, 0);
        if (bytes_received < 0) {
            perror("recv");
        } else {
            request[bytes_received] = '\0';
            //printf("Received request:\n%s\n", request);
            process_request(request, client_socket);
        }




        shutdown(client_socket, SHUT_RDWR);
        close(client_socket);
    }

    close(server_socket);
    free(file_map);
    return 0;
}