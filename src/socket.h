#ifndef SOCKET_H
#define SOCKET_H

/**
 * Create TCP socket and connect to server
 * Returns socket file descriptor on success, -1 on error
 */
int create_socket(const char *ip, int port);

/**
 * Close socket
 */
void close_socket(int sockfd);

#endif // SOCKET_H