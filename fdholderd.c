#include <sys/types.h>
#include <sys/socket.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <poll.h>
#include <linux/limits.h>

#include "config.h"

struct node_t
{
    struct node_t *next;
    char          *file;
    int            fd;
};

static void open_file(char *file, struct node_t **root)
{
    struct node_t *node;
    
    node = malloc(sizeof(*node));
    if (node == NULL)
    {
        perror("malloc");
        return;
    }

    node->file = strdup(file);
    if (node->file == NULL)
    {
        perror("strdup");
        goto strdup_failed;
    }

    node->fd = open(file, O_RDWR | O_NOCTTY);
    if (node->fd == -1)
    {
        perror("open");
        goto open_failed;
    }

    node->next = *root;
    *root = node;

    return;

open_failed:
    free(node->file);
strdup_failed:
    free(node);
}

static void close_file(char *file, struct node_t **root)
{
    struct node_t **pnode = root;
    struct node_t *node;

    while (*pnode)
    {
        node = *pnode;
        if (strcmp(file, node->file) == 0)
        {
            if (close(node->fd) == -1)
            {
                perror("close");
            }
            free(node->file);
            *pnode = node->next;
            free(node);
            break;
        }
        pnode = &node->next;
    }
}

static void get_data(int c, struct node_t **root)
{
    char buffer[PATH_MAX+2];
    int sz;
    int i;
    int left = sizeof(buffer);
    char *p = buffer;

    for (;;)
    {
        sz = read(c, p, left);
        if (sz == -1)
        {
            perror("read");
            return;
        }

        for (i=0; i<sz; ++i)
            if (p[i] == '\0')
                break;

        if (i < sz)
            break;

         left -= sz;

         if (left <= 0)
            return;

         p += sz;
    }

    p = buffer;

    if (p[0] == '-')
    {
        ++p;
        close_file(p, root);
    } else
    {
        if (p[0] == '+')
        {
            ++p;
            open_file(p, root);
        }
    }
}

static void remove_all_nodes(struct node_t **root)
{
    struct node_t *node;

    while (*root)
    {
        node = *root;
        
        if (close(node->fd) == -1)
        {
            perror("close");
        }
        free(node->file);
        *root = node->next;
        free(node);
    }
}

int main(void)
{
    const char socket_path[] = SOCKET_PATH;
    struct sockaddr_un addr;
    struct pollfd fds[2];
    struct signalfd_siginfo siginfo;
    sigset_t mask;
    int s;
    int c;
    int sigfd;
    int ret = 1;
    struct node_t *root = NULL;

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
    {
        perror("sigprocmask");
        goto sigprocmask_failed;
    }

    sigfd = signalfd(-1, &mask, 0);
    if (sigfd == -1)
    {
        perror("signalfd");
        goto signalfd_failed;
    }

    s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s == -1)
    {
        perror("socket");
        goto socket_failed;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(socket_path)-1);

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("bind");
        goto bind_failed;
    }

    if (listen(s, 0) == -1)
    {
        perror("listen");
        goto listen_failed;
    }

    fds[0].fd = s;
    fds[0].events = POLLIN | POLLRDNORM;
    fds[1].fd = sigfd;
    fds[1].events = POLLIN;

    ret = 0;

    for (;;)
    {
        if (poll(fds, sizeof(fds)/sizeof(*fds), -1) == -1)
        {
            perror("poll");
            ret = 1;
            break;
        }

        if (fds[0].revents & POLLIN)
        {
            c = accept(fds[0].fd, NULL, NULL);
            if (c == -1)
            {
                perror("accept");
            } else
            {
                get_data(c, &root);
                close(c);
            }
        }

        if (fds[1].revents & POLLIN)
        {
            if (read(fds[1].fd, &siginfo, sizeof(siginfo)) == sizeof(siginfo))
            {
                printf("\n%s\n", strsignal(siginfo.ssi_signo));
                break;
            }
        }
    }

    if (unlink(socket_path) == -1)
    {
        perror("unlink");
    }

    remove_all_nodes(&root);

listen_failed:
bind_failed:
    close(s);
socket_failed:
    close(sigfd);
signalfd_failed:
sigprocmask_failed:
    return ret;
}
