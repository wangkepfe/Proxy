#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "proxy_clientside.h"


/* startProxy
 *
 * Start the HTTP proxy listening on the given local port
 *
 * @param port Port number the proxy should listen on
 * @ret 0 on success
 *      1 if listening socket could not be opened
 */
int startProxy(const char *port)
{
        assert(port != NULL);

        printf("Starting proxy\n");

        Socket listen_socket;
        initSocket(&listen_socket);

        if(openListeningSocket(port, &listen_socket) != 0)
        {
                fprintf(stderr, "Could not open listening socket\n");
                return 1;
        }

        printf("Proxy listening on port %s\n", port);

        if(listenLoop(&listen_socket) != 0)
        {
                fprintf(stderr, "Accepting connection failed\n");
        }

        destroySocket(&listen_socket);

        return 0;
}





/* listenLoop
 * @param listen_sockfd Socket file descriptor to listen on
 *
 * Start listening for incoming connections on the given socket
 * For every incoming connection, spawn a new thread and read incoming data.
 * If the incoming data is a HTTP request, check if the request should be filtered
 * based on the requeste URL and if not, connect to the target server and forward the request.
 * The original request is modified to remove any possible hostname prefix from the
 * requested URL as well as to use 'Connection: close'
 * Also spawns a listener on the server side socket that calls a callback for returning the
 * data from the server to the client
 *
 */
int listenLoop(Socket *listen_sockfd)
{
        assert(listen_sockfd != NULL);

        while(1)
        {
                Socket client_sockfd;
                initSocket(&client_sockfd);

                if(acceptConnection(listen_sockfd, &client_sockfd) != 0)
                {
                        return -1;
                }

                if(!fork())
                {
                        // Child process
                        int exit_val = 0;
                        destroySocket(listen_sockfd);

                        exit_val = clientSession(&client_sockfd);

                        destroySocket(&client_sockfd);
                        exit(exit_val);
                }
                else
                {
                        // Parent process
                        destroySocket(&client_sockfd);
                }
        }
}
