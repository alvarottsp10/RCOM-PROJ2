/**
 * FTP Download Application
 * Lab 2 - Computer Networks
 * 
 * Single-file implementation based on RFC 959 (FTP) and RFC 1738 (URL)
 * 
 * Usage: ./download ftp://[<user>:<password>@]<host>/<url-path>
 * 
 * Features:
 * - URL parsing (ftp://[user:pass@]host/path)
 * - DNS resolution (gethostbyname)
 * - TCP sockets for control and data connections
 * - FTP protocol implementation (USER, PASS, PASV, RETR, QUIT)
 * - Passive mode for data transfer
 * - Binary mode file transfer
 */

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

/**
 * URL structure
 */
typedef struct {
    char user[MAX_LENGTH];
    char password[MAX_LENGTH];
    char host[MAX_LENGTH];
    char path[MAX_LENGTH];
    char filename[MAX_LENGTH];
    char ip[MAX_LENGTH];
} url_info;

/**
 * Extract filename from path
 */
void extract_filename(const char *path, char *filename) {
    const char *last_slash = strrchr(path, '/');
    if (last_slash != NULL) {
        strcpy(filename, last_slash + 1);
    } else {
        strcpy(filename, path);
    }
    
    if (strlen(filename) == 0) {
        strcpy(filename, "downloaded_file");
    }
}

/**
 * Resolve hostname to IP address using DNS
 */
int resolve_hostname(const char *host, char *ip) {
    struct hostent *h;
    
    printf("Resolving hostname '%s'...\n", host);
    
    if ((h = gethostbyname(host)) == NULL) {
        fprintf(stderr, "Error: Cannot resolve hostname '%s'\n", host);
        herror("gethostbyname()");
        return -1;
    }
    
    strcpy(ip, inet_ntoa(*((struct in_addr *)h->h_addr)));
    printf("Hostname: %s\n", h->h_name);
    printf("IP Address: %s\n", ip);
    
    return 0;
}

/**
 * Parse FTP URL
 * Format: ftp://[<user>:<password>@]<host>/<url-path>
 */
int parse_url(const char *url, url_info *info) {
    // Initialize with defaults
    strcpy(info->user, "anonymous");
    strcpy(info->password, "anonymous");
    memset(info->host, 0, MAX_LENGTH);
    memset(info->path, 0, MAX_LENGTH);
    memset(info->filename, 0, MAX_LENGTH);
    memset(info->ip, 0, MAX_LENGTH);
    
    // Check if URL starts with ftp://
    if (strncmp(url, "ftp://", 6) != 0) {
        fprintf(stderr, "Error: URL must start with ftp://\n");
        return -1;
    }
    
    // Skip "ftp://"
    const char *ptr = url + 6;
    char buffer[MAX_LENGTH * 2];
    strcpy(buffer, ptr);
    
    // Check for user:password@
    char *at_sign = strchr(buffer, '@');
    char *first_slash = strchr(buffer, '/');
    
    if (at_sign != NULL && (first_slash == NULL || at_sign < first_slash)) {
        // Has user:password
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
    
    // Extract host and path
    char *slash = strchr(ptr, '/');
    if (slash != NULL) {
        *slash = '\0';
        strcpy(info->host, ptr);
        strcpy(info->path, slash + 1);
    } else {
        strcpy(info->host, ptr);
        strcpy(info->path, "");
    }
    
    // Validate
    if (strlen(info->host) == 0) {
        fprintf(stderr, "Error: Invalid host\n");
        return -1;
    }
    
    if (strlen(info->path) == 0) {
        fprintf(stderr, "Error: Invalid path\n");
        return -1;
    }
    
    // Extract filename
    extract_filename(info->path, info->filename);
    
    // Resolve hostname to IP
    if (resolve_hostname(info->host, info->ip) < 0) {
        return -1;
    }
    
    return 0;
}

/**
 * Create TCP socket and connect to server
 */
int create_socket(const char *ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;
    
    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        return -1;
    }
    
    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);
    
    // Connect to server
    printf("Connecting to %s:%d...\n", ip, port);
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect()");
        close(sockfd);
        return -1;
    }
    
    printf("Connected to %s:%d\n", ip, port);
    return sockfd;
}

/**
 * Read FTP response from server
 * Returns response code (e.g., 220, 331, 230) or -1 on error
 */
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
            // Single-line response or last line of multi-line response
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

/**
 * Send FTP command to server
 */
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

/**
 * Login to FTP server
 */
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
    printf("\n");
    if (ftp_send_command(sockfd, "USER", user) < 0) {
        return -1;
    }
    
    code = ftp_read_response(sockfd, buffer, sizeof(buffer));
    if (code != 331) {
        fprintf(stderr, "Error: Expected 331, got %d\n", code);
        return -1;
    }
    
    // Send PASS
    printf("\n");
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

/**
 * Enter passive mode and get IP and port for data connection
 */
int ftp_pasv(int sockfd, char *ip, int *port) {
    char buffer[BUFFER_SIZE];
    int code;
    
    // Send PASV
    printf("\n");
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

/**
 * Download file from FTP server
 */
int ftp_retrieve(int control_sockfd, int data_sockfd, const char *path, const char *filename) {
    char buffer[BUFFER_SIZE];
    int code;
    
    // Send RETR
    printf("\n");
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
    
    printf("\nDownloading file '%s'...\n", filename);
    
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
        printf("\rReceived: %zu bytes", total_bytes);
        fflush(stdout);
    }
    printf("\n");
    
    fclose(fp);
    
    if (bytes < 0) {
        perror("read()");
        return -1;
    }
    
    printf("Download complete! Total bytes: %zu\n", total_bytes);
    
    // Read final response
    printf("\n");
    code = ftp_read_response(control_sockfd, buffer, sizeof(buffer));
    if (code != 226 && code != 426) {
        fprintf(stderr, "Warning: Unexpected response code %d\n", code);
    }
    
    return 0;
}

/**
 * Send QUIT command and close connection
 */
void ftp_quit(int sockfd) {
    char buffer[BUFFER_SIZE];
    
    printf("\n");
    ftp_send_command(sockfd, "QUIT", NULL);
    ftp_read_response(sockfd, buffer, sizeof(buffer));
    
    printf("Connection closed.\n");
}

/**
 * Print usage information
 */
void print_usage(const char *program_name) {
    printf("Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", program_name);
    printf("\nExamples:\n");
    printf("  %s ftp://ftp.netlab.fe.up.pt/pub/README\n", program_name);
    printf("  %s ftp://rcom:rcom@ftp.netlab.fe.up.pt/pipe.txt\n", program_name);
    printf("  %s ftp://demo:password@test.rebex.net/readme.txt\n", program_name);
}

/**
 * Main function
 */
int main(int argc, char **argv) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    url_info info;
    int control_sockfd = -1;
    int data_sockfd = -1;
    int ret = 0;
    
    printf("=== FTP Download Client ===\n\n");
    
    // Parse URL
    printf("Parsing URL...\n");
    if (parse_url(argv[1], &info) < 0) {
        fprintf(stderr, "Error: Failed to parse URL\n");
        return 1;
    }
    
    printf("User: %s\n", info.user);
    printf("Host: %s\n", info.host);
    printf("Path: %s\n", info.path);
    printf("Filename: %s\n\n", info.filename);
    
    // Open control connection
    printf("Opening control connection...\n");
    control_sockfd = create_socket(info.ip, FTP_PORT);
    if (control_sockfd < 0) {
        fprintf(stderr, "Error: Failed to connect to FTP server\n");
        return 1;
    }
    printf("\n");
    
    // Login
    printf("Logging in...\n");
    if (ftp_login(control_sockfd, info.user, info.password) < 0) {
        fprintf(stderr, "Error: Login failed\n");
        ret = 1;
        goto cleanup;
    }
    printf("\n");
    
    // Enter passive mode
    printf("Entering passive mode...\n");
    char pasv_ip[16];
    int pasv_port;
    if (ftp_pasv(control_sockfd, pasv_ip, &pasv_port) < 0) {
        fprintf(stderr, "Error: Failed to enter passive mode\n");
        ret = 1;
        goto cleanup;
    }
    printf("\n");
    
    // Open data connection
    printf("Opening data connection...\n");
    data_sockfd = create_socket(pasv_ip, pasv_port);
    if (data_sockfd < 0) {
        fprintf(stderr, "Error: Failed to open data connection\n");
        ret = 1;
        goto cleanup;
    }
    printf("\n");
    
    // Download file
    printf("Retrieving file...\n");
    if (ftp_retrieve(control_sockfd, data_sockfd, info.path, info.filename) < 0) {
        fprintf(stderr, "Error: Failed to download file\n");
        ret = 1;
        goto cleanup;
    }
    printf("\n");
    
    printf("=== Download successful! ===\n");
    printf("File saved as: %s\n", info.filename);
    
cleanup:
    // Close data connection first
    if (data_sockfd >= 0) {
        close(data_sockfd);
        data_sockfd = -1;
    }
    
    // Quit and close control connection
    if (control_sockfd >= 0) {
        ftp_quit(control_sockfd);
        close(control_sockfd);
        control_sockfd = -1;
    }
    
    return ret;
}