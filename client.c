/*
 * Library reservation client: TCP to server, line-based protocol matching server.c.
 * On exit prints: Session closed. Goodbye, <library_id>
 */

#include <arpa/inet.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 8080
#define BUF 512

static int read_line(int fd, char *out, size_t cap) {
    size_t i = 0;
    while (i + 1 < cap) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n < 0)
            return -1;
        if (n == 0)
            return (int)i == 0 ? -1 : 0;
        if (c == '\n')
            break;
        if (c != '\r')
            out[i++] = c;
    }
    out[i] = '\0';
    return 0;
}

int main(void) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    char id[BUF];
    printf("Enter your library ID: ");
    fflush(stdout);
    if (!fgets(id, (int)sizeof(id), stdin)) {
        close(sock);
        return 1;
    }
    id[strcspn(id, "\r\n")] = '\0';

    char msg[BUF + 16];
    snprintf(msg, sizeof(msg), "AUTH %s\n", id);
    if (write(sock, msg, strlen(msg)) < 0) {
        perror("write");
        close(sock);
        return 1;
    }

    char line[BUF];
    if (read_line(sock, line, sizeof(line)) != 0) {
        fprintf(stderr, "Server closed connection during authentication.\n");
        close(sock);
        return 1;
    }

    printf("Authentication: %s\n", line);
    if (strcmp(line, "OK") != 0) {
        printf("Session closed. Goodbye, %s\n", id);
        close(sock);
        return 0;
    }

    if (read_line(sock, line, sizeof(line)) != 0 || strcmp(line, "BOOKS") != 0) {
        fprintf(stderr, "Unexpected protocol from server (expected BOOKS).\n");
        close(sock);
        return 1;
    }

    printf("\nAvailable books:\n");
    for (;;) {
        if (read_line(sock, line, sizeof(line)) != 0) {
            fprintf(stderr, "Connection lost while reading book list.\n");
            close(sock);
            return 1;
        }
        if (strcmp(line, "END") == 0)
            break;
        printf("  - %s\n", line);
    }

    char title[BUF];
    printf("\nEnter book title to reserve (exact name): ");
    fflush(stdout);
    if (!fgets(title, (int)sizeof(title), stdin)) {
        close(sock);
        return 1;
    }
    title[strcspn(title, "\r\n")] = '\0';

    snprintf(msg, sizeof(msg), "RESERVE %s\n", title);
    if (write(sock, msg, strlen(msg)) < 0) {
        perror("write");
        close(sock);
        return 1;
    }

    if (read_line(sock, line, sizeof(line)) != 0) {
        fprintf(stderr, "No response for reservation.\n");
        close(sock);
        return 1;
    }

    if (strcmp(line, "RESERVED") == 0)
        printf("Server response: Reserved successfully.\n");
    else if (strcmp(line, "TAKEN") == 0)
        printf("Server response: Already reserved.\n");
    else
        printf("Server response: %s\n", line);

    close(sock);
    printf("Session closed. Goodbye, %s\n", id);
    return 0;
}
