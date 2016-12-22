#ifndef PROXY_H
#define PROXY_H

#include "util_socket.h"

// Maximum length that should be checked if it contains a HTTP header
#define MAX_HEADER_SIZE 8192

// Size of the buffer for reading data from sockets
#define RECEIVE_BUFFER_SIZE MAX_HEADER_SIZE


/* SessionInfo struct
 *
 * Helper struct for keeping track of client and server sockets in a session
 *
 * client_sockfd -> Holds the socket file descriptor for the client socket
 * server_sockfd -> Holds the socket file descriptor for the server socket
 */
typedef struct _session_info_
{
  Socket client_socket;
  Socket server_socket;
} SessionInfo;



int startProxy(const char *port);

int listenLoop(Socket *listen_sockfd);

#endif
