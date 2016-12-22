#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "proxy.h"
#include "midlayer.h"
#include "serverside.h"
#include "proxy_clientside.h"
#include "util_socket.h"



/* Define the keywords that should lead to blocking of a request/response
 * The word filter is case insensitive
 */
#define NUM_FILTERED_WORDS 8
const char *filtered_words[NUM_FILTERED_WORDS] =
        {
                "spongebob",
                "britney spears",
                "paris hilton",
                "norrkoping",
                "norrköping",
                "norrk%C3%B6ping",
                "norrk%C3%96ping",
                "norrkoeping"
        };

/* HTTP response string to return when a server response is blocked based on its content
 */
const char *filtered_redirect_content = "HTTP/1.1 301 Moved Permanently\r\nLocation: http://www.ida.liu.se/~TDTS04/labs/2011/ass2/error2.html\r\nConnection: close\r\n\r\n";



/* strcasestr
 *
 * Forward declaration for the GNU extenstion strcasestr for case insensitive substring search
 *
 * @param haystack String buffer to search in
 * @param needle Substring to search for
 * @ret Pointer to first occurrence of substring in the buffer, or NULL if substring not found
 */
char *strcasestr(const char *haystack, const char *needle);



/* forwardToServer
 *
 * Send the given buffer to the server using the provided socket
 * Filtering for HTTP requests is done on the client side
 *
 * @param buffer Buffer to send
 * @param buffer_len Length of the buffer
 * @param server_sockfd Server socket for sending data
 * @ret >0 if data was sent without errors
 *
 */
int forwardToServer(const char *buffer, size_t buffer_len, Socket *server_sockfd)
{
        assert(buffer != NULL);
        assert(server_sockfd != NULL);

        return sendData(server_sockfd, buffer, buffer_len);
}

/* forwardToClient
 *
 * Forward the received data from the server to the client.
 * Partial data is buffered first until HTTP header is found to decide if the content filter should be applied
 * to the response, or until more than MAX_HEADER_SIZE bytes have been read without finding a complete HTTP header,
 * in which case the response is discarded.
 *
 * @param recv_buffer Buffer containing (partial) received data from the server
 * @param recv_buffer_len Length of the received data
 * @param env Callback environment to imitate a closure (void* for abstraction)
 * @ret Whether any errors occured while forwarding the buffer to the client. (-1 on error, 1 on blocked response)
 *
 */
int forwardToClient(const char *recv_buffer, size_t recv_buffer_len, void *env)
{
        assert(recv_buffer != NULL);
        assert(env != NULL);
        MidlayerCallbackEnv *mid_env = (MidlayerCallbackEnv*)env;
        const char *str_to_send = NULL;
        size_t str_len = 0;
        int flush_buffer = 0;

        if(mid_env->apply_filter)
        {
                // If filter should be applied, keep buffering data until we have the whole response
                char *buffer_start =  extendBuffer(&(mid_env->cache_buffer), 
                                                   mid_env->cache_buffer_size, recv_buffer_len + 1);
                if(buffer_start == NULL)
                {
                        // Buffer extension failed
                        fprintf(stderr, "ERROR: Could not extend buffer for server response\n");
                        free(mid_env->cache_buffer);
                        mid_env->cache_buffer_size = 0;
                        return -1;
                }

                mid_env->cache_buffer_size += recv_buffer_len;

                memcpy(buffer_start, recv_buffer, recv_buffer_len);
                mid_env->cache_buffer[mid_env->cache_buffer_size] = '\0';


                // Check for HTTP header if it has not been received yet
                if(mid_env->have_header == 0)
                {
                        HTTPResponseHeader resp_header;
                        initResponseHeader(&resp_header);

                        if(parseResponseHeader(&resp_header, mid_env->cache_buffer) == 0)
                        {
                                // HTTP header found, check if it should be filtered
                                mid_env->have_header = 1;
                                mid_env->apply_filter = shouldApplyContentFilterHeader(&resp_header);
                        }

                        freeResponseHeader(&resp_header);

                        /* If already more than MAX_HEADER_SIZE bytes have been buffered 
                           but no header found yet, abort */
                        if((mid_env->cache_buffer_size > MAX_HEADER_SIZE) 
                            && (mid_env->have_header == 0))
                        {
                                free(mid_env->cache_buffer);
                                mid_env->cache_buffer_size = 0;
                                return -1;
                        }
                }
        }

        if(mid_env->apply_filter == 0)
        {
                // Don't apply filter, just forward data as soon as we get it
                // If we have buffered data, flush buffer first
                if(mid_env->cache_buffer_size != 0)
                {
                        str_to_send = mid_env->cache_buffer;
                        str_len = mid_env->cache_buffer_size;
                        flush_buffer = 1;
                }
                else
                {
                        str_to_send = recv_buffer;
                        str_len = recv_buffer_len;
                }
        }
        else if(recv_buffer_len == 0)
        {
                // End of response and filter should be applied
                mid_env->block_response = applyFilter(mid_env->cache_buffer);

                if(mid_env->block_response)
                {
                        // Block response
                        printf("Response from server blocked because of filtered words in content\n");
                        str_to_send = filtered_redirect_content;
                        str_len = strlen(filtered_redirect_content);
                }
                else
                {
                        // Forward response
                        str_to_send = mid_env->cache_buffer;
                        str_len = mid_env->cache_buffer_size;
                }
        }


        // Forward data to client
        if(str_to_send != NULL)
        {
                sendData(mid_env->client_sockfd, str_to_send, str_len);
        }

        // If the allocated buffer was flushed while returning data, clear it now
        if(flush_buffer)
        {
                free(mid_env->cache_buffer);
                mid_env->cache_buffer = NULL;
                mid_env->cache_buffer_size = 0;
        }

        // If the response is blocked, return 1 so calling process knows it can abort read
        if(mid_env->block_response)
        {
                return 1;
        }

        return 0;
}


/* extendBuffer
 *
 * Extend a buffer by a number of bytes while preserving the original content
 *
 * @param buffer Buffer to extend (can be NULL)
 * @param buffer_size Buffer size before extension (must be 0 if buffer=NULL)
 * @param extend_by Number of bytes the buffer should be extended by
 * @ret Pointer to the start of the buffer extension or NULL if (re)allocation failed. If NULL is returned, the original buffer has not been modified.
 */
char * extendBuffer(char **buffer, size_t buffer_size, size_t extend_by)
{
        char *temp = NULL;
        char *ext_buffer_start = NULL;

        if(buffer_size == 0)
        {
                temp = malloc(extend_by);
                if(temp == NULL)
                {
                        // Error allocating buffer
                        return NULL;
                }

                ext_buffer_start = temp;
        }
        else
        {
                temp = realloc(*buffer, buffer_size + extend_by);
                if(temp == NULL)
                {
                        // Error realloc
                        return NULL;
                }

                ext_buffer_start = temp + buffer_size;
        }

        *buffer = temp;
        return ext_buffer_start;
}


/* shouldApplyContentFilterHeader
 *
 * Check if the content filter should be applied to a HTTP response by
 * checking if the returned data is text and not encoded
 *
 * @param resp_header HTTP response header of the response to check
 * @ret True if the response should be checked for blocked words
 *
 */
int shouldApplyContentFilterHeader(const HTTPResponseHeader *resp_header)
{
        int is_text = 0;
        int is_encoded = 0;

        const char* content_type_key = "Content-Type";
        const char* content_encoding_key = "Content-Encoding";

        const char *content_type_value = getValue(&(resp_header->fields), content_type_key);
        if(content_type_value != NULL)
        {
                is_text = (strstr(content_type_value, "text") != NULL ? 1 : 0);
        }

        const char *content_encoding_value = getValue(&(resp_header->fields), content_encoding_key);
        if(content_encoding_value != NULL)
        {
                is_text = (strstr(content_type_value, "identity") != NULL ? 1 : 0);
        }

        return is_text && (!is_encoded);
}



/* applyFilter
 *
 * Check if the given string buffer contains blocked words
 *
 * @param buffer String buffer that should be searched for blocked words
 * @ret True when the buffer contains blocked words
 *
 */
int applyFilter(const char *buffer)
{
        for(size_t i = 0; i < NUM_FILTERED_WORDS; ++i)
        {
                if(containsWord(buffer, filtered_words[i]))
                {
                        printf("Found filtered word: %s\n", filtered_words[i]);
                        return 1;
                }
        }

        return 0;
}

/* containsWord
 *
 * Search buffer for occurences of a substring
 * Uses the GNU extension strcasestr for case insensitive comparison
 *
 * @param buffer String buffer to be searched for the given substring/word
 * @param word Substring to search for
 * @ret True if substring found in buffer
 *
 */
int containsWord(const char *buffer, const char *word)
{
        return (strcasestr(buffer, word) != NULL);
}


/* initMidlayerCallbackEnv
 *
 * Initialize a callback environment
 *
 * @param env Callback environment to initialize
 * @param socket_fd Socket file descriptor to use in the callback
 */
void initMidlayerCallbackEnv(MidlayerCallbackEnv *env, Socket *socket_fd)
{
        env->client_sockfd = socket_fd;
        env->call_counter = 0;
        env->block_response = 0;
        env->apply_filter = 1;
        env->have_header = 0;

        env->cache_buffer_size = 0;
        env->cache_buffer = NULL;
}


/* destroyMidlayerCallbackEnv
 *
 * Destroy a callback environment after it is no longer being used
 *
 * @param env Callback environment to destroy
 */
void destroyMidlayerCallbackEnv(MidlayerCallbackEnv *env)
{
        assert(env != NULL);

        if(env->cache_buffer_size != 0)
        {
                free(env->cache_buffer);
        }
}
