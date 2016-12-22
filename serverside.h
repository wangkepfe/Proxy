#ifndef SERVERSIDE_H
#define SERVERSIDE_H

#include "http.h"
#include "util_socket.h"

// Size of the serverside receive buffer
#define SERVERSIDE_RECEIVE_BUFFER_SIZE 8192

/* ServerListenerEnv struct
 *
 * Struct holding the environment for the serverside listening thread
 * Holds references to the client and server sockets as well as an indication whether
 * content filter should be applied to the response.
 *
 * client_socket_ -> Pointer to client socket
 * server_socker_ -> Pointer to server socket
 * apply_filter   -> Whether the content filter should be applied
 *
 */
typedef struct _server_listener_env_
{
        Socket *client_socket_;
        Socket *server_socket_;
        int apply_filter_;
} ServerListenerEnv;

void initServerListenerEnv(ServerListenerEnv *env, Socket *client_socket, Socket *server_socket, int filter);
void destroyServerListenerEnv(ServerListenerEnv *env);

void* serverListener(void *s_env);

int readFromSocket(const Socket *socket_fd, int (*callback)(const char *_response_buffer_, size_t _response_len_, void *_callback_env_), void *callback_env);

#endif
