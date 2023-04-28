#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <string>
#include <algorithm>
#include <thread>
#include <mutex>

#define PORT 8080
#define HTTP_VERSION "HTTP/1.0"
#define CONTENT_TYPE_HTML "text/html"
#define CONTENT_TYPE_TXT "text/plain"
#define CONTENT_TYPE_JPEG "image/jpeg"
#define CONTENT_TYPE_PNG "image/png"

std::mutex mtx;

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
    std::string path_lower(path);
    std::transform(path_lower.begin(), path_lower.end(), path_lower.begin(), ::tolower);
    DIR *dir = opendir(".");
    if (dir == NULL) {
        send_404(client_socket);
        return;
    }
    std::string file_name;
    bool file_found = false;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcasecmp(ent->d_name, path_lower.c_str()) == 0) {
            file_name = ent->d_name;
            file_found = true;
            break;
        }
    }
    closedir(dir);
    if (!file_found) {
        send_404(client_socket);
        return;
    }
    const char *content_type;
    std::string file_ext = file_name.substr(file_name.find_last_of('.'));
    if (file_ext == ".html" || file_ext == ".htm") {
        content_type = CONTENT_TYPE_HTML;
    } else if (file_ext == ".txt") {
        content_type = CONTENT_TYPE_TXT;
    } else if (file_ext == ".jpg" || file_ext == ".jpeg") {
        content_type = CONTENT_TYPE_JPEG;
    } else if (file_ext == ".png") {
        content_type = CONTENT_TYPE_PNG;
    } else {
        send_404(client_socket);
        return;
    }
    FILE *file = fopen(file_name.c_str(), "rb");
    if (file == NULL) {
        send_404(client_socket);
        return;
    }
    send_file(file, client_socket, content_type);
    fclose(file);
}

void handle_client(int client_socket) {
    char request[4096];
    ssize_t bytes_received = recv(client_socket, request, sizeof(request) - 1, 0);
    if (bytes_received < 0) {
        perror("recv");
    } else {
        request[bytes_received] = '\0';
        process_request(request, client_socket);
    }

    shutdown(client_socket, SHUT_RDWR);
    close(client_socket);
}







int main() {
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
    while (1) {
        int client_socket;
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("accept");
            continue;
        }
        std::thread client_thread(handle_client, client_socket);
        client_thread.detach();
    }
    close(server_socket);
    return 0;
}