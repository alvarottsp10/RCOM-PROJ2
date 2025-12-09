#ifndef FTP_H
#define FTP_H

#include <stddef.h>

#define FTP_PORT 21
#define BUFFER_SIZE 4096

/**
 * Read FTP response from server
 * Returns response code (e.g., 220, 331, 230) or -1 on error
 */
int ftp_read_response(int sockfd, char *buffer, size_t buffer_size);

/**
 * Send FTP command to server
 * Returns 0 on success, -1 on error
 */
int ftp_send_command(int sockfd, const char *command, const char *arg);

/**
 * Login to FTP server
 * Returns 0 on success, -1 on error
 */
int ftp_login(int sockfd, const char *user, const char *password);

/**
 * Set transfer mode to binary
 * Returns 0 on success, -1 on error
 */
int ftp_set_binary_mode(int sockfd);

/**
 * Enter passive mode and get IP and port for data connection
 * Returns 0 on success, -1 on error
 */
int ftp_pasv(int sockfd, char *ip, int *port);

/**
 * Download file from FTP server
 * Returns 0 on success, -1 on error
 */
int ftp_retrieve(int control_sockfd, int data_sockfd, const char *path, const char *filename);

/**
 * Send QUIT command and close connection
 */
void ftp_quit(int sockfd);

#endif // FTP_H