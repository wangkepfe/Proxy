#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>


#include "proxy_clientside.h"
#include "proxy.h"
#include "util_socket.h"
#include "midlayer.h"
#include "serverside.h"
#include "http.h"

const char *filtered_redirect_url = "HTTP/1.1 301 Moved Permanently\r\nLocation: http://www.ida.liu.se/~TDTS04/labs/2011/ass2/error1.html\r\n\r\n";
const char *error_entity_too_large = "HTTP/1.1 413 Entity Too Large\r\n\r\n";
char *conn_est = "HTTP/1.1 200 Connection Established\r\n\r\n";
const char *HTTP_DEFAULT_PORT = "80";




/* checkHeaderExtractHost
 *
 * Check if the target buffer contains a HTTP header. If yes parse the header and extract the 
 * target host name and port.
 *
 * @param buffer Buffer to be checked for a HTTP request header. Must be null terminated.
 * @ret request_header Request header struct to be filled in with the parsed data when HTTP request 
 *      header found in buffer
 * @ret hostname_target Pointer to buffer where parsed hostname should be stored. 
 *      Will be alloceded by the function.
 * @ret port_target Pointer to buffer where parsed port number should be stored. 
 *      Will be allocated by the function.
 * @ret 0 if HTTP request header was found in buffer and hostname/port were extracted.
 *     -1 if no HTTP request header could be found or if an error occured. Hostname and port buffer 
 *     are not modified in this case.
 */
int checkHeaderExtractHost(const char *buffer, HTTPRequestHeader *request_header, char **hostname_target, char **port_target)
{
        assert(buffer != NULL);
        assert(hostname_target != NULL);
        assert(port_target != NULL);


        int retval = 0;
        char *hostname = NULL;
        char *port = NULL;
        size_t host_len = 0;

        retval = parseRequestHeader(request_header, buffer);
        if(retval == 0)
        {
                // HTTP Header found

                // Extract host name
                const char *host_value = getValue(&(request_header->fields), "Host");
                if(host_value == NULL)
                {
                        // Error, HTTP request did not contain host field
                        fprintf(stderr, "Request did not include 'Host' field\n");
                        retval = -1;
                        goto error_host;
                }

                if(setString(&hostname, host_value) != 0)
                {
                        // Error allocationg host name
                        retval = -1;
                        goto error_host;
                }

                // Extract port when specified
                char *port_delim = strstr(hostname, ":");
                if(port_delim != NULL)
                {
                        host_len = port_delim-hostname;
                        hostname[host_len] = '\0';
                }
                else
                {
                        host_len = strlen(hostname);
                }

                if(port_delim != NULL)
                {
                        // Target port specified
                        size_t port_len = strlen(port_delim + 1);
                        if(port_len != 0)
                        {
                                // Extract port
                                if(setStringN(&port, port_delim + 1, port_len) != 0)
                                {
                                        retval = -1;
                                        goto error_port_alloc;
                                }
                        }
                }


                if(port == NULL) // No special port, use default
                {
                        port = malloc(sizeof(char) * (strlen(HTTP_DEFAULT_PORT) + 1));
                        if(port == NULL)
                        {
                                retval = -1;
                                goto error_port_alloc;
                        }

                        strcpy(port, HTTP_DEFAULT_PORT);
                }

                // Set target hostname and port
                *hostname_target = hostname;
                *port_target = port;
        }

error_port_alloc:
error_host:
        return retval;
}


/* extractResource
 *
 * Extract the requested resource/file path from an url, given the hostname and port number
 *
 * @param resource URL string the path should be extracted from
 * @param hostname Known hostname for the url
 * @param port Known port number for the url
 * @ret Pointer to start of the resource/path in the provided url
 */
const char * extractResource(const char *resource, const char *hostname, const char *port)
{
        size_t host_len = strlen(hostname);
        size_t port_len = strlen(port);

        char *http_pos = strstr(resource, "http://");
        char *host_pos = strstr(resource, hostname);

        // True if resource starts with http://
        int start_http = (http_pos == resource);

        // True if resource starts with <hostname> or http://<hostname>
        int start_host = ((host_pos == resource) ||
                          ((host_pos == (resource + strlen("http://"))) && start_http));

        /* True if hostname is followed by :<port>
           No out of bound access possible because ':' compares agains '\0' char at string end 
           -> early abort or strcmp ends at string end */
        int start_port = (start_host &&
                          (*(host_pos + host_len) == ':') &&
                          (strcmp(host_pos + host_len + 1, port)));

        // Calculate start of requested resource without hostname/port
        const char * resource_start = ( start_port ? host_pos + host_len + 1 + port_len :
                                      ( start_host ? host_pos + host_len :
                                                     resource ));


        return resource_start;
}



/* clientSession
 *
 * Start a connection session. Read the HTTP request from the client and forward it to 
 * the server and vice versa.
 *
 * @param client_socket Socket with opened client connection
 * @ret -1 on error
 */
int clientSession(Socket *client_socket)
{
        assert(client_socket != NULL);
        assert(client_socket->open_);

        int ret_val = 0;

        // Create and init session info struct
        SessionInfo session_info;
        session_info.client_socket = *client_socket;
        initSocket(&(session_info.server_socket));


        //############################
        // Read and forward data

        const size_t header_buffer_len = RECEIVE_BUFFER_SIZE;
        char header_buffer[header_buffer_len + 1];
        memset(header_buffer, '\0', header_buffer_len + 1);
        size_t received_bytes = 0;

        int header_found = 0;
        HTTPRequestHeader request_header;
        initRequestHeader(&request_header);

        char *hostname = NULL;
        char *port = NULL;

        int block_request = 0;
        int conn_request = 0;

        int modify_request = 1;

        while(!header_found)
        {
                if(!client_socket->open_)
                {
                        // Client socket closed before header found
                        fprintf(stderr, "ERROR: Client socket closed before header was received\n");
                        ret_val = -1;
                        goto error_header_read;
                }

                if((header_buffer_len - received_bytes) == 0)
                {
                        // No space left and header not yet received
                        fprintf(stderr, 
                                "ERROR: Exceeded max header size without finding HTTP header\n");
                        sendData(client_socket, error_entity_too_large, strlen(error_entity_too_large));
                        ret_val = -1;
                        goto error_header_read;
                }

                ssize_t read_stat = readData(client_socket, header_buffer + received_bytes, 
                                             header_buffer_len - received_bytes);
                if(read_stat < 0)
                {
                        // Read error
                        fprintf(stderr, "ERROR: Error while reading from client socket\n");
                        ret_val = -1;
                        goto error_header_read;
                }

                received_bytes += read_stat;

                // Check for header
                int header_status = checkHeaderExtractHost(header_buffer, &request_header, 
                                                           &hostname, &port);
                header_found = (header_status == 0);
        }

        // Check if request should be blocked
        block_request = applyFilter(header_buffer);
        if(block_request)
        {
                // Bad words found, block request
                printf("Found bad words in client request, blocking\n");
                sendData(client_socket, filtered_redirect_url, strlen(filtered_redirect_url));
                ret_val = 0;
                goto end_url_blocked;
        }


        // Have HTTP header and extracted hostname and port
        // Establish connection to server
        printf("Connecting to host: %s port: %s\n", hostname, port);
        Socket server_socket;
        int con_stat = initServerConnection(hostname, port, &server_socket);
        if(con_stat == -1)
        {
                fprintf(stderr, "Failed to open connection to server\n");
                ret_val = -1;
                goto error_connection;
        }


        // Check if request type is CONNECT
        conn_request = (strstr(request_header.request_info.req_type, "CONNECT") != NULL);
        if(conn_request)
        {
                printf("CONNECT request\n");
                modify_request = 0;
        }


        // Modify request if neccessary
        size_t header_len_pre_modifcation = requestHeaderLength(&request_header);
        if(modify_request)
        {
                // Modify Connection field
                if(setValue(&(request_header.fields), "Connection", "close", strlen("close")) == -1)
                {
                        int add_cclose = addField(&(request_header.fields), "Connection", 
                                                  strlen("Connection"), "close", strlen("close"));
                        if(add_cclose != 0)
                        {
                                fprintf(stderr, "ERROR: Failed to add Connection: close field\n");
                        }
                }

                // Shorten to only contain requested resource
                const char *extracted_resource = extractResource(request_header.request_info.resource,
                                                                 hostname, port);
                printf("Requesting resource: %s%s\n", hostname, extracted_resource);
                setString(&(request_header.request_info.resource), extracted_resource);
        }


        // Send connection establised response for CONNECT request
        if(conn_request)
        {
                sendData(client_socket, conn_est , strlen(conn_est));
        }


        // Create server listener thread
        ServerListenerEnv s_env;
        initServerListenerEnv(&s_env, client_socket, &server_socket, !conn_request);

        pthread_t server_thread_id;
        pthread_create(&server_thread_id, NULL, serverListener, &s_env);


        // Send header data
        if((modify_request) && (!conn_request))
        {
                // Send (modified) header
                char *serialized_request = NULL;
                size_t serialized_request_length = 0;
                if(serializeRequestHeader(&request_header, &serialized_request, 
                                          &serialized_request_length) != 0)
                {
                        fprintf(stderr, "ERROR: Failed to serialize request\n");
                        ret_val = -1;
                        goto error_serialization;
                }
                sendData(&server_socket, serialized_request, serialized_request_length);
                free(serialized_request);

                // Send already received non-header bytes
                sendData(&server_socket, header_buffer + header_len_pre_modifcation, 
                         received_bytes - header_len_pre_modifcation);
        }
        else if(!conn_request)
        {
                // Send all received bytes
                sendData(&server_socket, header_buffer, received_bytes);
        }


        // Continuously forward data from client to server
        while(1)
        {
                pthread_mutex_lock(&(server_socket.mutex_));
                if(!server_socket.open_)
                {
                        pthread_mutex_unlock(&(server_socket.mutex_));
                        break;
                }
                pthread_mutex_unlock(&(server_socket.mutex_));

                pthread_mutex_lock(&(client_socket->mutex_));
                if(!client_socket->open_)
                {
                        pthread_mutex_unlock(&(client_socket->mutex_));
                        break;
                }
                pthread_mutex_unlock(&(client_socket->mutex_));

                char read_buffer[RECEIVE_BUFFER_SIZE];
                ssize_t read_bytes = readData(client_socket, read_buffer, RECEIVE_BUFFER_SIZE);
                if(read_bytes == -1)
                {
                        ret_val = -1;
                        break;
                }
                if(sendData(&server_socket, read_buffer, read_bytes) == -1)
                {
                        ret_val = -1;
                        break;
                }
        }

        
        
        // Cleanup
error_serialization:
        pthread_join(server_thread_id, NULL);
        destroyServerListenerEnv(&s_env);
        destroySocket(&server_socket);
end_url_blocked:
error_connection:
        free(hostname);
        free(port);
error_header_read:
        freeRequestHeader(&request_header);
        return ret_val;
}
