#include <stdio.h>
#include <libgen.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/un.h>
#include <sys/socket.h>

#include "config.h"

int main(int argc, char **argv)
{
    const char socket_path[] = SOCKET_PATH;
    struct sockaddr_un addr;
    int s;
    int ret = 1;

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <file>\n", basename(argv[0]));
        return 1;
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

    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("connect");
        goto connect_failed;
    }

    if (write(s, argv[1], strlen(argv[1])+1) == -1)
    {
        perror("write");
        goto write_failed;
    }

    ret = 0;

write_failed:
connect_failed:
    close(s);
socket_failed:
    return ret;
}
