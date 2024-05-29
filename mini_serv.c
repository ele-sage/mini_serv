#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MSG "client %d: %s\n"
#define LONG_MSG "client %d: %s%s\n"
#define JOIN "server: client %d just arrived\n"
#define LEFT "server: client %d just left\n"
#define FATAL "Fatal error\n"
#define WRONG_NUM "Wrong number of arguments\n"
#define MAX_FD 1024
#define MAX_CLIENT 100

const size_t    PREPEND_LEN = strlen(MSG) + 10;
const int       SEND_SIZE = 120000;
const int       RECV_SIZE = 120000 - PREPEND_LEN;
int gid = 0;

typedef struct s_client
{
    char    *queue;
    int     id;
} t_client;

fd_set current, read_fds, write_fds;
char send_queue[SEND_SIZE];
char recv_queue[RECV_SIZE];
t_client clients[MAX_FD];

void free_clients()
{
    for (int fd = 3; fd < MAX_FD; fd++)
    {
        if (clients[fd].queue)
            free(clients[fd].queue);
        close(fd);
    }
}

void std_err(char *str)
{
    if (str)
        write(2, str, strlen(str));
    free_clients();
    exit(1);
}

char *str_join(char *buf, char *add)
{
    char *newbuf;
    int len;

    if (buf == 0)
        len = 0;
    else
        len = strlen(buf);
    newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
    if (newbuf == 0)
        std_err(FATAL);
    newbuf[0] = 0;
    if (buf != 0)
        strcat(newbuf, buf);
    free(buf);
    strcat(newbuf, add);
    return (newbuf);
}

void broadcast(int senderfd, int maxfd, char *queue)
{
    size_t queue_len = strlen(queue);

    if (!queue_len)
        return;
    for (int fd = 0; fd < maxfd + 1; fd++)
        if (fd != senderfd && FD_ISSET(fd, &write_fds))
            if (send(fd, queue, queue_len, 0) == -1)
                std_err(FATAL);
    if (queue[0])
        queue[0] = '\0';
}

void format_msg(int senderfd, int bytes, int maxfd)
{
    int i = 0, j = 0;
    char *tmp_queue = NULL;

    while (i + j < bytes)
    {
        while (recv_queue[i + j] && recv_queue[i + j] != '\n')
            j++;
        if (recv_queue[i + j] != '\n')
            clients[senderfd].queue = str_join(clients[senderfd].queue, recv_queue + i);
        else
        {
            recv_queue[i + j] = '\0';
            if (clients[senderfd].queue)
            {
                size_t tmp_len = PREPEND_LEN + strlen(clients[senderfd].queue) + strlen(recv_queue + i);
                tmp_queue = calloc(tmp_len, sizeof(char));
                if (!tmp_queue)
                    std_err(FATAL);
                sprintf(tmp_queue, LONG_MSG, clients[senderfd].id, clients[senderfd].queue, recv_queue + i);
                broadcast(senderfd, maxfd, tmp_queue);
                free(tmp_queue);
                free(clients[senderfd].queue);
                clients[senderfd].queue = NULL;
            }
            else
            {
                sprintf(send_queue, MSG, clients[senderfd].id, recv_queue + i);
                broadcast(senderfd, maxfd, send_queue);
            }
            i += j + 1;
            j = 0;
        }
    }
}

void clear_client(int fd)
{
    if (clients[fd].queue)
        free(clients[fd].queue);
    clients[fd].queue = NULL;
    clients[fd].id = 0;
}

void add_client(int fd, int maxfd)
{
    clear_client(fd);
    clients[fd].id = gid++;
    FD_SET(fd, &current);
    sprintf(send_queue, JOIN, clients[fd].id);
    broadcast(fd, maxfd, send_queue);
}

void remove_client(int fd, int maxfd)
{
    FD_CLR(fd, &current);
    close(fd);
    sprintf(send_queue, LEFT, clients[fd].id);
    broadcast(fd, maxfd, send_queue);
    clear_client(fd);
}

int main(int argc, char **argv)
{
    int sock, maxfd;
    struct sockaddr_in addr;
    socklen_t   len;

    if (argc != 2)
        std_err(WRONG_NUM);

    maxfd = sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        std_err(FATAL);

    bzero(&addr, sizeof(addr));
    bzero(clients, sizeof(t_client) * MAX_CLIENT);
    FD_ZERO(&current);
    FD_SET(sock, &current);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(argv[1]));

    if ((bind(sock, (const struct sockaddr *)&addr, sizeof(addr))) != 0 || listen(sock, MAX_CLIENT) != 0)
        std_err(FATAL);

    while (1)
    {
        read_fds = write_fds = current;
        if (select(maxfd + 1, &read_fds, &write_fds, 0, 0) == -1) continue;
        for (int fd = sock; fd < maxfd + 1; fd++)
        {
            if (FD_ISSET(fd, &read_fds))
            {
                if (fd == sock)
                {
                    int new_fd = accept(sock, (struct sockaddr *) &addr, &len);
                    if (new_fd == -1) continue;
                    if (new_fd > maxfd)
                        maxfd = new_fd;
                    add_client(new_fd, maxfd);
                }
                else
                {
                    bzero(recv_queue, RECV_SIZE);
                    int bytes = recv(fd, recv_queue, RECV_SIZE, 0);
                    if (bytes <= 0)
                        remove_client(fd, maxfd);
                    else
                        format_msg(fd, bytes, maxfd);
                }
            }
        }
    }
    free_clients();
    close(sock);
    return 0;
}