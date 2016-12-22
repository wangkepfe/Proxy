#ifndef UTIL_SOCKET_H
#define UTIL_SOCKET_H

#include <netinet/in.h>
#include <pthread.h>

#define CONNECTION_BACKLOG 10

/* Socket wrapper struct
 *
 * Adds indication of socket status
 */
typedef struct _socket_
{
        int fd_;
        int open_;
        pthread_mutex_t mutex_;
} Socket;

void initSocket(Socket *socket);
void destroySocket(Socket *socket);
void closeSocket(Socket *socket);

ssize_t readData(Socket *socket, char *target_buffer, size_t buffer_size);

ssize_t sendData(Socket *socket, const char *buffer, size_t len);

void *get_in_addr(struct sockaddr *sa);

int openListeningSocket(const char *port, Socket *socket_ret);

int acceptConnection(const Socket *listen_sockfd, Socket *client_sockfd);

int initServerConnection(const char *hostname, const char *port, Socket *ret_socket);

#endif
