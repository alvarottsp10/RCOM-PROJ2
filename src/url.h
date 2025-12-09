#ifndef URL_H
#define URL_H

#define MAX_LENGTH 256

typedef struct {
    char user[MAX_LENGTH];
    char password[MAX_LENGTH];
    char host[MAX_LENGTH];
    char path[MAX_LENGTH];
    char filename[MAX_LENGTH];
    char ip[MAX_LENGTH];
} url_info;

/**
 * Parse FTP URL in format: ftp://[<user>:<password>@]<host>/<url-path>
 * Returns 0 on success, -1 on error
 */
int parse_url(const char *url, url_info *info);

/**
 * Resolve hostname to IP address
 * Returns 0 on success, -1 on error
 */
int resolve_hostname(const char *host, char *ip);

/**
 * Extract filename from path
 */
void extract_filename(const char *path, char *filename);

#endif // URL_H