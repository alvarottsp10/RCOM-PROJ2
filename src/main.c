#include <stdio.h>
#include <stdlib.h>
#include "url.h"
#include "socket.h"
#include "ftp.h"

void print_usage(const char *program_name) {
    printf("Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", program_name);
    printf("\nExamples:\n");
    printf("  %s ftp://ftp.netlab.fe.up.pt/pipe.txt\n", program_name);
    printf("  %s ftp://rcom:rcom@ftp.netlab.fe.up.pt/pipe.txt\n", program_name);
}

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

    // Set binary mode
    printf("Setting binary mode...\n");
    if (ftp_set_binary_mode(control_sockfd) < 0) {
        fprintf(stderr, "Error: Failed to set binary mode\n");
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

cleanup:
    // Close data connection first
    if (data_sockfd >= 0) {
        close_socket(data_sockfd);
        data_sockfd = -1;
    }

    // Quit and close control connection
    if (control_sockfd >= 0) {
        ftp_quit(control_sockfd);
        close_socket(control_sockfd);
        control_sockfd = -1;
    }

    return ret;
}