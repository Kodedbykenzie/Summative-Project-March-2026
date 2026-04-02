/*
 * Digital Library Reservation — TCP server (pthread per client, mutex-protected
 * shared book state and active-user list). Protocol (line-oriented, newline-terminated):
 *
 *   Client -> AUTH <library_id>\n
 *   Server -> OK\n | FAIL\n
 *   If OK:
 *     Server -> BOOKS\n
 *     Server -> <title>\n  (one line per available book)
 *     Server -> END\n
 *   Client -> RESERVE <title>\n
 *   Server -> RESERVED\n | TAKEN\n | UNKNOWN\n
 *
 * Unauthenticated clients receive only FAIL and are disconnected. Book
 * reservation and active-user updates run under one mutex to prevent double
 * booking and torn list updates. At most MAX_SESSIONS concurrent authenticated
 * handlers are allowed; extra connections get BUSY and are closed.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 8080
#define MAX_SESSIONS 5
#define MAX_BOOKS 8
#define MAX_ID_LEN 32
#define BUF 512

static const char *valid_ids[] = {"LIB101", "LIB102", "LIB103", "LIB104"};
static const int n_valid = (int)(sizeof(valid_ids) / sizeof(valid_ids[0]));

static char book_titles[MAX_BOOKS][64] = {
    "Signals-Systems",
    "Operating-Systems",
    "Computer-Networks",
    "Embedded-Design",
};
static int book_reserved[MAX_BOOKS];
static int n_books = 4;

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int active_sessions;
static char active_users[MAX_SESSIONS][MAX_ID_LEN];
static int n_active_users;

static int authenticate(const char *id) {
    for (int i = 0; i < n_valid; i++) {
        if (strcmp(id, valid_ids[i]) == 0)
            return 1;
    }
    return 0;
}

static ssize_t write_all(int fd, const void *buf, size_t n) {
    const char *p = (const char *)buf;
    size_t left = n;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        p += w;
        left -= (size_t)w;
    }
    return (ssize_t)n;
}

static int read_line(int fd, char *out, size_t cap) {
    size_t i = 0;
    while (i + 1 < cap) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
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

static int find_book(const char *title) {
    for (int i = 0; i < n_books; i++) {
        if (strcmp(book_titles[i], title) == 0)
            return i;
    }
    return -1;
}

static void print_server_status(void) {
    pthread_mutex_lock(&g_lock);
    printf("\n--- Server status ---\nActive users (%d): ", n_active_users);
    for (int i = 0; i < n_active_users; i++)
        printf("%s ", active_users[i]);
    printf("\nBooks:\n");
    for (int i = 0; i < n_books; i++)
        printf("  [%d] %-24s %s\n", i, book_titles[i], book_reserved[i] ? "reserved" : "available");
    printf("---------------------\n\n");
    fflush(stdout);
    pthread_mutex_unlock(&g_lock);
}

typedef struct {
    int fd;
} client_ctx_t;

static void *handle_client(void *arg) {
    client_ctx_t *ctx = (client_ctx_t *)arg;
    int fd = ctx->fd;
    free(ctx);

    char line[BUF];
    char user[MAX_ID_LEN] = "";

    if (read_line(fd, line, sizeof(line)) != 0) {
        close(fd);
        return NULL;
    }

    if (strncmp(line, "AUTH ", 5) != 0) {
        write_all(fd, "FAIL\n", 5);
        close(fd);
        return NULL;
    }

    const char *id = line + 5;
    if (!authenticate(id)) {
        write_all(fd, "FAIL\n", 5);
        printf("Auth failed for '%s'\n", id);
        fflush(stdout);
        close(fd);
        return NULL;
    }

    pthread_mutex_lock(&g_lock);
    if (active_sessions >= MAX_SESSIONS) {
        pthread_mutex_unlock(&g_lock);
        write_all(fd, "BUSY\n", 5);
        close(fd);
        return NULL;
    }
    active_sessions++;
    if (n_active_users < MAX_SESSIONS) {
        strncpy(active_users[n_active_users], id, MAX_ID_LEN - 1);
        active_users[n_active_users][MAX_ID_LEN - 1] = '\0';
        n_active_users++;
    }
    strncpy(user, id, MAX_ID_LEN - 1);
    user[MAX_ID_LEN - 1] = '\0';
    pthread_mutex_unlock(&g_lock);

    write_all(fd, "OK\n", 3);
    printf("User authenticated: %s\n", user);
    fflush(stdout);

    pthread_mutex_lock(&g_lock);
    write_all(fd, "BOOKS\n", 6);
    for (int i = 0; i < n_books; i++) {
        if (!book_reserved[i]) {
            char lineout[BUF];
            int len = snprintf(lineout, sizeof(lineout), "%s\n", book_titles[i]);
            if (len > 0 && len < (int)sizeof(lineout))
                write_all(fd, lineout, (size_t)len);
        }
    }
    write_all(fd, "END\n", 4);
    pthread_mutex_unlock(&g_lock);

    if (read_line(fd, line, sizeof(line)) != 0) {
        pthread_mutex_lock(&g_lock);
        active_sessions--;
        for (int i = 0; i < n_active_users; i++) {
            if (strcmp(active_users[i], user) == 0) {
                memmove(&active_users[i], &active_users[i + 1],
                        (size_t)(n_active_users - i - 1) * sizeof(active_users[0]));
                n_active_users--;
                break;
            }
        }
        pthread_mutex_unlock(&g_lock);
        close(fd);
        return NULL;
    }

    if (strncmp(line, "RESERVE ", 8) != 0) {
        write_all(fd, "UNKNOWN\n", 8);
    } else {
        const char *title = line + 8;
        pthread_mutex_lock(&g_lock);
        int bi = find_book(title);
        if (bi < 0)
            write_all(fd, "UNKNOWN\n", 8);
        else if (book_reserved[bi])
            write_all(fd, "TAKEN\n", 6);
        else {
            book_reserved[bi] = 1;
            write_all(fd, "RESERVED\n", 9);
            printf("Reservation: %s reserved '%s'\n", user, book_titles[bi]);
            fflush(stdout);
        }
        pthread_mutex_unlock(&g_lock);
    }

    pthread_mutex_lock(&g_lock);
    active_sessions--;
    for (int i = 0; i < n_active_users; i++) {
        if (strcmp(active_users[i], user) == 0) {
            memmove(&active_users[i], &active_users[i + 1],
                    (size_t)(n_active_users - i - 1) * sizeof(active_users[0]));
            n_active_users--;
            break;
        }
    }
    pthread_mutex_unlock(&g_lock);

    print_server_status();
    close(fd);
    return NULL;
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(server_fd, 16) < 0) {
        perror("listen");
        return 1;
    }

    printf("Library server listening on port %d (max %d concurrent sessions).\n", PORT, MAX_SESSIONS);
    fflush(stdout);

    for (;;) {
        struct sockaddr_in cli;
        socklen_t clen = sizeof(cli);
        int cfd = accept(server_fd, (struct sockaddr *)&cli, &clen);
        if (cfd < 0) {
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;
        }

        client_ctx_t *ctx = malloc(sizeof(*ctx));
        if (!ctx) {
            close(cfd);
            continue;
        }
        ctx->fd = cfd;

        pthread_t th;
        if (pthread_create(&th, NULL, handle_client, ctx) != 0) {
            free(ctx);
            close(cfd);
            continue;
        }
        pthread_detach(th);
    }
}
