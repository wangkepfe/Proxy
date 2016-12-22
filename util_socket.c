#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

#include "util_socket.h"


// Get IP address, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa)
{
        if (sa->sa_family == AF_INET) {
                return &(((struct sockaddr_in*)sa)->sin_addr);
        }

        return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* initSocket
 *
 * Initialize a Socket struct
 *
 * @param socket The struct to initialize
 */
void initSocket(Socket *socket)
{
        assert(socket != NULL);
        socket->fd_ = -1;
        socket->open_ = 0;

        if(pthread_mutex_init(&(socket->mutex_), NULL) != 0)
        {
                fprintf(stderr, "Socket mutex init failed\n");
                exit(-1);
        }
}


/* destroySocket
 *
 * Destroy a socket after it is no longer used. Closes the socket if it is open.
 *
 * @param socket The socket to destroy
 */
void destroySocket(Socket *socket)
{
        assert(socket != NULL);

        closeSocket(socket);
        socket->fd_ = -1;

        pthread_mutex_destroy(&(socket->mutex_));
}

/* closeSocket
 *
 * Close a socket
 *
 * @param socket The socket to close
 */
void closeSocket(Socket *socket)
{
        assert(socket != NULL);

        if(socket->open_)
        {
                close(socket->fd_);
                socket->open_ = 0;
        }
}


/* sendData
 *
 * Send all provided data to the socket
 *
 * @param socket Socket to send the data to
 * @param buffer Data to send
 * @param len Length of the data to send
 * @ret length of sent data
 *      -1 on error
 */
ssize_t sendData(Socket *socket, const char *buffer, size_t len)
{
        assert(socket != NULL);

        ssize_t retval = 0;

        if(!socket->open_)
        {
                return -1;
        }

        size_t total = 0;
        int left = len;
        int n;

        pthread_mutex_lock(&(socket->mutex_));
        while(total < len)
        {
                errno = 0;
                n = send(socket->fd_, buffer + total, left, 0);

                if(n == -1)
                {
                        if(errno == ECONNRESET)
                        {
                                // Remote closed connection
                                closeSocket(socket);
                        }
                        retval = -1;
                        goto error_send;
                }

                total += n;
                left -= n;
        }
        pthread_mutex_unlock(&(socket->mutex_));


        retval = total;
error_send:
        return retval;
}

/* readData
 *
 * Read data from a socket, up to the length of the provided buffer. Blocks when no data available
 *
 * @param socket The socket to read from
 * @param read_buffer Buffer the read data should be stored in
 * @param buffer_size Size of the buffer
 */
ssize_t readData(Socket *socket, char *read_buffer, size_t buffer_size)
{
        assert(socket != NULL);
        assert(read_buffer != NULL);

        if(!socket->open_)
        {
                return -1;
        }

        pthread_mutex_lock(&(socket->mutex_));
        errno = 0;
        ssize_t read_len = recv(socket->fd_, read_buffer , buffer_size, MSG_DONTWAIT);
        int errno_saved = errno;
        if(read_len == 0)
        {
                // Connection closed -> end of data
                closeSocket(socket);
        }
        pthread_mutex_unlock(&(socket->mutex_));

        if(read_len == -1)
        {
                if((errno_saved == EAGAIN) || (errno_saved == EWOULDBLOCK))
                {
                        read_len = 0;
                }
                else
                {
                        // Error
                        fprintf(stderr, "Read error (-1)\n");
                        return -1;
                }
        }

        return read_len;
}

/* openListeningSocket
 *
 * Open a local TCP socket for listening
 *
 * @param port Local port that should be opened for listening
 * @ret socket_ret Pointer to socket file descriptor that will hold the opened port
 * @ret 0 if the socket could be opened without errors, -1 otherwise
 * If successful, the socket file descriptor in socket_ret will be overwritten
 * to hold the newly opened socket
 *
 */
int openListeningSocket(const char *port, Socket *socket_ret)
{
        assert(port != NULL);

        int retval = 0;
        Socket listen_socket;
        initSocket(&listen_socket);
        struct addrinfo hints;
        struct addrinfo *servinfo;
        struct addrinfo *p;
        int getaddrinfo_status;
        const int optval = 1;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE; // use my IP

        getaddrinfo_status = getaddrinfo(NULL, port, &hints, &servinfo);
        if(getaddrinfo_status != 0)
        {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(getaddrinfo_status));
                retval = -1;
                goto err_getaddrinfo;
        }
        // loop through all the results and bind to the first we can
        for(p = servinfo; p != NULL; p = p->ai_next)
        {
                listen_socket.fd_ = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                if (listen_socket.fd_ == -1)
                {
                        perror("server: socket");
                        continue;
                }

                if (setsockopt(listen_socket.fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1)
                {
                        perror("setsockopt");
                        retval = -1;
                        goto err_setsockopt;
                }
                if (bind(listen_socket.fd_, p->ai_addr, p->ai_addrlen) == -1)
                {
                        close(listen_socket.fd_);
                        perror("server: bind");
                        continue;
                }

                break;
        }


        if (p == NULL)
        {
                fprintf(stderr, "server: failed to bind\n");
                retval = -1;
                goto err_no_bind;
        }
        if (listen(listen_socket.fd_, CONNECTION_BACKLOG) == -1)
        {
                perror("listen");
                retval = -1;
                goto err_listen;
        }

        listen_socket.open_ = 1;
        *socket_ret = listen_socket;

err_listen:
err_no_bind:
err_setsockopt:
        freeaddrinfo(servinfo); // all done with this structure
err_getaddrinfo:

        return retval;
}


/* acceptConnection
 *
 * Accept an incoming connection on a listening socket
 *
 * @param listen_sockfd Listening socket with incoming connection
 * @ret client_sockfd Socket file dexcriptor that will hold the socket for the accepted connection
 * @ret 0 if no errors were encountered while accepting the conection, -1 otherwise
 * If no errors were encountered, the socket file descriptor pointed to by client_sockfd
 * will be set to the socket for the accepted incoming connection
 *
 */
int acceptConnection(const Socket *listen_socket, Socket *client_socket)
{
        assert(listen_socket != NULL);
        assert(client_socket != NULL);

        struct sockaddr_storage their_addr; // connector's address information
        socklen_t sin_size;
        Socket temp_socket;
        initSocket(&temp_socket);

        sin_size = sizeof(their_addr);

        //pthread_mutex_lock(&(listen_socket->mutex_));
        temp_socket.fd_ = accept(listen_socket->fd_, (struct sockaddr *)&their_addr, &sin_size);
        //pthread_mutex_unlock(&(listen_socket->mutex_));
        if (temp_socket.fd_ == -1)
        {
                perror("accept");
                return -1;
        }

        temp_socket.open_ = 1;
        *client_socket = temp_socket;


        // Print status on incoming connection
        char s[INET6_ADDRSTRLEN];

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof(s));
        printf("Received connection from %s\n", s);

        return 0;
}


/* initServerConnection
 *
 * Initiate a TCP connection to the server with the given hostname and port
 *
 * @param hostname Host name of the server
 * @param port Target port
 * @ret File descriptor of the socket on success, -1 on failure
 */
int initServerConnection(const char *hostname, const char *port, Socket *ret_socket)
{
        assert(hostname != NULL);
        assert(port != NULL);

        Socket server_socket;
        initSocket(&server_socket);

        int getaddr_status;
        struct addrinfo conntype;
        struct addrinfo *result_list;
        struct addrinfo *addrnode;

        // Fill conntype struct with information about the type of connection we want
        memset(&conntype, 0, sizeof(struct addrinfo));
        conntype.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
        conntype.ai_socktype = SOCK_STREAM; // Stream socket
        conntype.ai_flags = 0;
        conntype.ai_protocol = 0; // Don't care about the protocol

        getaddr_status = getaddrinfo(hostname, port, &conntype, &result_list);
        if(getaddr_status != 0)
        {
                // Lookup failed
                return -1;
        }

        // Try connecting to addresses provided by getaddrinfo
        for(addrnode = result_list; addrnode != NULL; addrnode = addrnode->ai_next)
        {
                server_socket.fd_ = socket(addrnode->ai_family, addrnode->ai_socktype, 
                                           addrnode->ai_protocol);
                if(server_socket.fd_ == -1)
                {
                        continue; // Failed to create socket
                }

                if(connect(server_socket.fd_, addrnode->ai_addr, addrnode->ai_addrlen) != -1)
                {
                        break; // Connection successful
                }

                close(server_socket.fd_); // Socket created but connection failed
        }

        if(addrnode == NULL)
        {
                // No connection to any address succeeded
                return -1;
        }

        // Address information is no longer needed, connection already established
        freeaddrinfo(result_list);

        server_socket.open_ = 1;
        *ret_socket = server_socket;
        return 0;
}
