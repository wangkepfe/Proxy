#ifndef PROXY_CLIENTSIDE_H
#define PROXY_CLIENTSIDE_H

#include <netinet/in.h>

#include "util_socket.h"
#include "proxy.h"
#include "http.h"



int checkHeaderExtractHost(const char *buffer, HTTPRequestHeader *request_header, char **hostname_target, char **port_target);

const char * extractResource(const char *resource, const char *hostname, const char *port);

int clientSession(Socket *client_sockfd);

#endif
