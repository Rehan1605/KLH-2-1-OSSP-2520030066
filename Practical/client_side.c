#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>

#define SOCKET_PATH "/tmp/my_socket"

int main()
{
    int client_fd;
    struct sockaddr_un address;

    client_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, SOCKET_PATH);

    connect(client_fd,
            (struct sockaddr *)&address,
            sizeof(address));

    write(client_fd, "Hello Server", 12);

    printf("Message sent to server.\n");

    close(client_fd);

    return 0;
}
