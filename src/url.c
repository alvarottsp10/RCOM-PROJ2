#include "url.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>

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

int resolve_hostname(const char *host, char *ip) {
    struct hostent *h;

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

void extract_filename(const char *path, char *filename) {
    const char *last_slash = strrchr(path, '/');
    if (last_slash != NULL) {
        strcpy(filename, last_slash + 1);
    } else {
        strcpy(filename, path);
    }

    // If no filename, use default
    if (strlen(filename) == 0) {
        strcpy(filename, "downloaded_file");
    }
}