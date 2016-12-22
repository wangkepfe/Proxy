#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "serverside.h"
#include "http.h"
#include "util_socket.h"
#include "midlayer.h"


/* serverListener
 *
 * Read the server response and forward it to the client. To be executed in a separate thread.
 * Closes the server socket when response was blocked by the content filter.
 *
 * @param s_env ServerListenerEnv containing references to the client and server sockets as well 
 *              as indicating whether the content filter should be applied to the response
 */
void* serverListener(void *s_env)
{
        assert(s_env != NULL);
        ServerListenerEnv *env = (ServerListenerEnv *)s_env;

        /* Create callback environment to pass on the client socket and
           allow the callback to save state between partial reads */
        MidlayerCallbackEnv mid_callback_env;
        initMidlayerCallbackEnv(&mid_callback_env, env->client_socket_);

        if(!env->apply_filter_)
        {
                mid_callback_env.apply_filter = 0;
        }

        int read_stat = readFromSocket(env->server_socket_, forwardToClient, &mid_callback_env);
        if(read_stat != 0)
        {
                // Close server socket
                pthread_mutex_lock(&(env->server_socket_->mutex_));
                closeSocket(env->server_socket_);
                pthread_mutex_unlock(&(env->server_socket_->mutex_));
        }

        // Clean up unneeded resourced from parent process

        destroyMidlayerCallbackEnv(&mid_callback_env);

        return NULL;
}


/* initServerListenerEnv
 *
 * Initialize the environment that is passed to the server listener thread
 *
 * @param env Environment to initialize
 * @param client_socket Pointer to the client socket for the session
 * @param server_socket Pointer to the server socket for the session
 * @param filter Whether the content filter should be applied or not
 */
void initServerListenerEnv(ServerListenerEnv *env, 
                           Socket *client_socket, 
                           Socket *server_socket, 
                           int filter)
{
        assert(env != NULL);
        assert(client_socket != NULL);
        assert(server_socket != NULL);

        env->client_socket_ = client_socket;
        env->server_socket_ = server_socket;
        env->apply_filter_ = filter;
}

/* destroyServerListenerEnv
 *
 * Destroy the target ServerListenerEnvironment struct
 * (currently no special functionality required)
 *
 * @param env Environment to destroy
 */
void destroyServerListenerEnv(ServerListenerEnv *env)
{
        assert(env != NULL);
}


/* readFromSocket
 *
 * Read from the given socket in a loop until it is closed. Call provided callback for every 
 * partial read
 *
 * @param socket_fd Socket to read from
 * @param callback Callback to call when data has been read
 * @param callback_env Callback environment to simulate closure
 */
int readFromSocket(const Socket *socket_fd, int (*callback)(const char *_response_buffer_, 
                                                            size_t _response_len_, 
                                                            void *_callback_env_), 
                   void *callback_env)
{
        assert(socket_fd != NULL);
        assert(callback != NULL);
        assert(callback_env != NULL);

        const size_t read_buffer_len = SERVERSIDE_RECEIVE_BUFFER_SIZE;
        char read_buffer[read_buffer_len + 1];
        memset(read_buffer, '\0', read_buffer_len + 1);
        size_t free_len = read_buffer_len;
        size_t total_len = 0;
        int read_id = 0;


        while(1)
        {
                ssize_t read_len = read(socket_fd->fd_, read_buffer + read_buffer_len - free_len , 
                                        free_len);
                if(read_len == -1)
                {
                        // Error
                        fprintf(stderr, "Read error (-1)\n");
                        return -1;
                }

                // Read successful
                free_len -= read_len;
                total_len += read_len;

                // Forward data to client when buffer full or connection closed
                int callback_stat = callback(read_buffer, read_buffer_len - free_len, callback_env);
                if(callback_stat != 0)
                {
                        printf("Aborting read from server because callback returned != 0\n");
                        return callback_stat;
                }

                ++read_id;
                free_len = read_buffer_len;


                if(read_len == 0)
                {
                        // Connection closed -> end of data
                        return 0;
                }
        }
}
