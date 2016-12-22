#ifndef MIDLAYER_H
#define MIDLAYER_H

#include "http.h"

/* MidlayerCallbackEnv
 *
 * Struct containing state for the midlayer callback to simulate a closure
 *
 * client_sockfd     -> Socket file descriptor to use for returning data to client
 * call_counter      -> Number of times the callback has been called
 * cache_buffer      -> Buffer for caching incomplete responses while waiting for 
 *                      HTTP header/end of response to apply filter
 * cache_buffer_size -> Size of the cache buffer
 * have_header       -> Indicates that a HTTP header has already been found
 * block_response    -> Indicates that the response should be blocked
 * apply_filter      -> Indicates that the response should be checked for blocked words
 */
typedef struct _midlayer_callback_env_
{
  Socket *client_sockfd;
  int call_counter;
  char *cache_buffer;
  size_t cache_buffer_size;
  int have_header;
  int block_response;
  int apply_filter;
} MidlayerCallbackEnv;


void initMidlayerCallbackEnv(MidlayerCallbackEnv *env, Socket *socket_fd);
void destroyMidlayerCallbackEnv(MidlayerCallbackEnv *env);


int forwardToServer(const char *buffer, size_t buffer_len, Socket *server_sockfd);
int forwardToClient(const char *recv_buffer, size_t recv_buffer_len, void *env);


int shouldApplyContentFilterHeader(const HTTPResponseHeader *resp_header);

int applyFilter(const char *buffer);
int containsWord(const char *buffer, const char *word);

char * extendBuffer(char **buffer, size_t buffer_size, size_t extend_by);

#endif
