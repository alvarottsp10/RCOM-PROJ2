#include "ftp.h"
#include "socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int ftp_read_response(int sockfd, char *buffer, size_t buffer_size) {
    memset(buffer, 0, buffer_size);
    FILE *fp = fdopen(dup(sockfd), "r");
    if (fp == NULL) {
        perror("fdopen()");
        return -1;
    }

    int code = 0;
    char line[BUFFER_SIZE];

    // Read all lines of the response
    while (fgets(line, sizeof(line), fp) != NULL) {
        printf("%s", line);
        
        // Parse response code (first 3 digits)
        if (strlen(line) >= 3 && line[3] == ' ') {
            sscanf(line, "%d", &code);
            strcat(buffer, line);
            break;
        } else if (strlen(line) >= 3 && line[3] == '-') {
            // Multi-line response, continue reading
            sscanf(line, "%d", &code);
            strcat(buffer, line);
        } else {
            strcat(buffer, line);
        }
    }

    fclose(fp);
    return code;
}

int ftp_send_command(int sockfd, const char *command, const char *arg) {
    char buffer[BUFFER_SIZE];
    
    if (arg != NULL && strlen(arg) > 0) {
        snprintf(buffer, sizeof(buffer), "%s %s\r\n", command, arg);
    } else {
        snprintf(buffer, sizeof(buffer), "%s\r\n", command);
    }

    printf(">>> %s", buffer);

    size_t len = strlen(buffer);
    if (write(sockfd, buffer, len) != (ssize_t)len) {
        perror("write()");
        return -1;
    }

    return 0;
}

int ftp_login(int sockfd, const char *user, const char *password) {
    char buffer[BUFFER_SIZE];
    int code;

    // Read welcome message
    code = ftp_read_response(sockfd, buffer, sizeof(buffer));
    if (code != 220) {
        fprintf(stderr, "Error: Expected 220, got %d\n", code);
        return -1;
    }

    // Send USER
    if (ftp_send_command(sockfd, "USER", user) < 0) {
        return -1;
    }

    code = ftp_read_response(sockfd, buffer, sizeof(buffer));
    if (code != 331) {
        fprintf(stderr, "Error: Expected 331, got %d\n", code);
        return -1;
    }

    // Send PASS
    if (ftp_send_command(sockfd, "PASS", password) < 0) {
        return -1;
    }

    code = ftp_read_response(sockfd, buffer, sizeof(buffer));
    if (code != 230) {
        fprintf(stderr, "Error: Login failed (code %d)\n", code);
        return -1;
    }

    printf("Login successful!\n");
    return 0;
}

int ftp_set_binary_mode(int sockfd) {
    char buffer[BUFFER_SIZE];
    int code;

    // Send TYPE I (binary mode)
    if (ftp_send_command(sockfd, "TYPE", "I") < 0) {
        return -1;
    }

    code = ftp_read_response(sockfd, buffer, sizeof(buffer));
    if (code != 200) {
        fprintf(stderr, "Warning: Failed to set binary mode (code %d)\n", code);
        return -1;
    }

    printf("Binary mode enabled.\n");
    return 0;
}

int ftp_pasv(int sockfd, char *ip, int *port) {
    char buffer[BUFFER_SIZE];
    int code;

    // Send PASV
    if (ftp_send_command(sockfd, "PASV", NULL) < 0) {
        return -1;
    }

    code = ftp_read_response(sockfd, buffer, sizeof(buffer));
    if (code != 227) {
        fprintf(stderr, "Error: Expected 227, got %d\n", code);
        return -1;
    }

    // Parse PASV response: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
    char *start = strchr(buffer, '(');
    if (start == NULL) {
        fprintf(stderr, "Error: Invalid PASV response\n");
        return -1;
    }
    start++;

    int h1, h2, h3, h4, p1, p2;
    if (sscanf(start, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) != 6) {
        fprintf(stderr, "Error: Failed to parse PASV response\n");
        return -1;
    }

    // Build IP address
    snprintf(ip, 16, "%d.%d.%d.%d", h1, h2, h3, h4);
    
    // Calculate port
    *port = p1 * 256 + p2;

    printf("Passive mode: IP=%s, Port=%d\n", ip, *port);

    return 0;
}

int ftp_retrieve(int control_sockfd, int data_sockfd, const char *path, const char *filename) {
    char buffer[BUFFER_SIZE];
    int code;

    // Send RETR
    if (ftp_send_command(control_sockfd, "RETR", path) < 0) {
        return -1;
    }

    code = ftp_read_response(control_sockfd, buffer, sizeof(buffer));
    if (code != 150 && code != 125) {
        fprintf(stderr, "Error: Expected 150/125, got %d\n", code);
        return -1;
    }

    // Open file for writing
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        perror("fopen()");
        return -1;
    }

    printf("Downloading file '%s'...\n", filename);

    // Read data from data socket
    ssize_t bytes;
    size_t total_bytes = 0;
    while ((bytes = read(data_sockfd, buffer, sizeof(buffer))) > 0) {
        if (fwrite(buffer, 1, bytes, fp) != (size_t)bytes) {
            perror("fwrite()");
            fclose(fp);
            return -1;
        }
        total_bytes += bytes;
    }

    fclose(fp);

    if (bytes < 0) {
        perror("read()");
        return -1;
    }

    printf("Download complete! Total bytes: %zu\n", total_bytes);

    // Read final response (226 Transfer complete or 426 Connection closed)
    // Both codes indicate successful file transfer
    code = ftp_read_response(control_sockfd, buffer, sizeof(buffer));
    if (code != 226 && code != 426) {
        fprintf(stderr, "Warning: Unexpected response code %d\n", code);
    }

    return 0;
}

void ftp_quit(int sockfd) {
    char buffer[BUFFER_SIZE];
    
    ftp_send_command(sockfd, "QUIT", NULL);
    ftp_read_response(sockfd, buffer, sizeof(buffer));
    
    printf("Connection closed.\n");
}