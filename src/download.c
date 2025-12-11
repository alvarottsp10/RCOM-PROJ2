#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define FTP_PORT 21
#define MAX_LENGTH 256
#define BUFFER_SIZE 4096

typedef struct {
    char user[MAX_LENGTH];
    char password[MAX_LENGTH];
    char host[MAX_LENGTH];
    char path[MAX_LENGTH];
    char filename[MAX_LENGTH];
    char ip[MAX_LENGTH];
} url_info;

void extract_filename(const char *path, char *filename) {
    const char *last_slash = strrchr(path, '/');
    if (last_slash != NULL) {
        strcpy(filename, last_slash + 1);
    } else {
        strcpy(filename, path);
    }
    
    if (strlen(filename) == 0) {
        strcpy(filename, "file");
    }
}

int resolve_hostname(const char *host, char *ip) {
    struct hostent *h;
    
    h = gethostbyname(host);
    if (h == NULL) {
        herror("gethostbyname");
        return -1;
    }
    
    strcpy(ip, inet_ntoa(*((struct in_addr *)h->h_addr)));
    return 0;
}

int parse_url(const char *url, url_info *info) {
    strcpy(info->user, "anonymous");
    strcpy(info->password, "anonymous");
    memset(info->host, 0, MAX_LENGTH);
    memset(info->path, 0, MAX_LENGTH);
    memset(info->filename, 0, MAX_LENGTH);
    memset(info->ip, 0, MAX_LENGTH);
    
    if (strncmp(url, "ftp://", 6) != 0) {
        fprintf(stderr, "URL must start with ftp://\n");
        return -1;
    }
    
    const char *ptr = url + 6;
    char buffer[MAX_LENGTH * 2];
    strcpy(buffer, ptr);
    
    char *at_sign = strchr(buffer, '@');
    char *first_slash = strchr(buffer, '/');
    
    if (at_sign != NULL && (first_slash == NULL || at_sign < first_slash)) {
        *at_sign = '\0';
        char *colon = strchr(buffer, ':');
        
        if (colon != NULL) {
            *colon = '\0';
            strcpy(info->user, buffer);
            strcpy(info->password, colon + 1);
        } else {
            strcpy(info->user, buffer);
        }
        
        ptr = at_sign + 1;
    } else {
        ptr = buffer;
    }
    
    char *slash = strchr(ptr, '/');
    if (slash != NULL) {
        *slash = '\0';
        strcpy(info->host, ptr);
        strcpy(info->path, slash + 1);
    } else {
        strcpy(info->host, ptr);
        strcpy(info->path, "");
    }
    
    if (strlen(info->host) == 0 || strlen(info->path) == 0) {
        fprintf(stderr, "Invalid URL\n");
        return -1;
    }
    
    extract_filename(info->path, info->filename);
    
    if (resolve_hostname(info->host, info->ip) < 0) {
        return -1;
    }
    
    return 0;
}

int create_socket(const char *ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);
    
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

int read_response(int sockfd, char *buffer, size_t buffer_size) {
    memset(buffer, 0, buffer_size);
    FILE *fp = fdopen(dup(sockfd), "r");
    if (fp == NULL) {
        perror("fdopen");
        return -1;
    }
    
    int code = 0;
    char line[BUFFER_SIZE];
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strlen(line) >= 3 && line[3] == ' ') {
            sscanf(line, "%d", &code);
            strcat(buffer, line);
            break;
        } else if (strlen(line) >= 3 && line[3] == '-') {
            sscanf(line, "%d", &code);
            strcat(buffer, line);
        } else {
            strcat(buffer, line);
        }
    }
    
    fclose(fp);
    return code;
}

int send_command(int sockfd, const char *command, const char *arg) {
    char buffer[BUFFER_SIZE];
    
    if (arg != NULL && strlen(arg) > 0) {
        snprintf(buffer, sizeof(buffer), "%s %s\r\n", command, arg);
    } else {
        snprintf(buffer, sizeof(buffer), "%s\r\n", command);
    }
    
    size_t len = strlen(buffer);
    if (write(sockfd, buffer, len) != (ssize_t)len) {
        perror("write");
        return -1;
    }
    
    return 0;
}

int login_ftp(int sockfd, const char *user, const char *password) {
    char buffer[BUFFER_SIZE];
    int code;
    
    code = read_response(sockfd, buffer, sizeof(buffer));
    if (code != 220) {
        return -1;
    }
    
    if (send_command(sockfd, "USER", user) < 0) {
        return -1;
    }
    
    code = read_response(sockfd, buffer, sizeof(buffer));
    if (code != 331) {
        return -1;
    }
    
    if (send_command(sockfd, "PASS", password) < 0) {
        return -1;
    }
    
    code = read_response(sockfd, buffer, sizeof(buffer));
    if (code != 230) {
        return -1;
    }
    
    return 0;
}

int passive_mode(int sockfd, char *ip, int *port) {
    char buffer[BUFFER_SIZE];
    int code;
    
    if (send_command(sockfd, "PASV", NULL) < 0) {
        return -1;
    }
    
    code = read_response(sockfd, buffer, sizeof(buffer));
    if (code != 227) {
        return -1;
    }
    
    char *start = strchr(buffer, '(');
    if (start == NULL) {
        return -1;
    }
    start++;
    
    int h1, h2, h3, h4, p1, p2;
    if (sscanf(start, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) != 6) {
        return -1;
    }
    
    snprintf(ip, 16, "%d.%d.%d.%d", h1, h2, h3, h4);
    *port = p1 * 256 + p2;
    
    return 0;
}

int retrieve_file(int control_sock, int data_sock, const char *path, const char *filename) {
    char buffer[BUFFER_SIZE];
    int code;
    
    if (send_command(control_sock, "RETR", path) < 0) {
        return -1;
    }
    
    code = read_response(control_sock, buffer, sizeof(buffer));
    if (code != 150 && code != 125) {
        return -1;
    }
    
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        perror("fopen");
        return -1;
    }
    
    ssize_t bytes;
    size_t total = 0;
    while ((bytes = read(data_sock, buffer, sizeof(buffer))) > 0) {
        if (fwrite(buffer, 1, bytes, fp) != (size_t)bytes) {
            perror("fwrite");
            fclose(fp);
            return -1;
        }
        total += bytes;
    }
    
    fclose(fp);
    
    if (bytes < 0) {
        perror("read");
        return -1;
    }
    
    printf("Downloaded: %zu bytes\n", total);
    
    read_response(control_sock, buffer, sizeof(buffer));
    
    return 0;
}

void quit_ftp(int sockfd) {
    char buffer[BUFFER_SIZE];
    send_command(sockfd, "QUIT", NULL);
    read_response(sockfd, buffer, sizeof(buffer));
}

void print_usage(const char *program) {
    printf("Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", program);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    url_info info;
    int control_sock = -1;
    int data_sock = -1;
    int ret = 0;
    
    if (parse_url(argv[1], &info) < 0) {
        return 1;
    }
    
    control_sock = create_socket(info.ip, FTP_PORT);
    if (control_sock < 0) {
        return 1;
    }
    
    if (login_ftp(control_sock, info.user, info.password) < 0) {
        ret = 1;
        goto cleanup;
    }
    
    char pasv_ip[16];
    int pasv_port;
    if (passive_mode(control_sock, pasv_ip, &pasv_port) < 0) {
        ret = 1;
        goto cleanup;
    }
    
    data_sock = create_socket(pasv_ip, pasv_port);
    if (data_sock < 0) {
        ret = 1;
        goto cleanup;
    }
    
    if (retrieve_file(control_sock, data_sock, info.path, info.filename) < 0) {
        ret = 1;
        goto cleanup;
    }
    
cleanup:
    if (data_sock >= 0) {
        close(data_sock);
    }
    
    if (control_sock >= 0) {
        quit_ftp(control_sock);
        close(control_sock);
    }
    
    return ret;
}