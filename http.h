#ifndef HTTP_H
#define HTTP_H

#include "util.h"

// Error return codes
#define E_NO_MATCH -1
#define E_ALLOC -2
#define E_REG_COMP -3
#define E_NOT_HTTP -4


//#############################
// Request


/* HTTPRequestInfo
 *
 * Struct for storing information found in the initial line of a HTTP request
 *
 * req_type     -> Type of the request (e.g. GET)
 * resource     -> Requested resource
 * http_version -> HTTP version used in the request
 */
typedef struct _http_request_info_
{
  char *req_type;
  char *resource;
  char *http_version;
} HTTPRequestInfo;

/* HTTPRequestHeader
 *
 * Struct describing a HTTP request header
 *
 * request_info -> Holds information about the HTTP request
 * fields       -> Key-Value array with all fields of the HTTP request header
 */
typedef struct _http_request_header_
{
  HTTPRequestInfo request_info;
  KeyValueArray fields;
} HTTPRequestHeader;


void initRequestHeader(HTTPRequestHeader *header);
void freeRequestHeader(HTTPRequestHeader *header);

int parseRequestHeader(HTTPRequestHeader *header, const char *buffer);
int parseRequestLine(HTTPRequestInfo *req_info, const char *buffer);
int serializeRequestHeader(const HTTPRequestHeader *request_header, char **target_buffer, size_t *written_length);
size_t requestHeaderLength(const HTTPRequestHeader *request_header);


//#############################
// Response

/* HTTPResponseInfo
 *
 * Struct for storing information found in the initial line of a HTTP response
 *
 * http_version -> HTTP version of the response (e.g. 1.1)
 * status_code  -> Status code of the response (e.g. 200)
 * reason       -> Reason for the HTTP response (e.g. OK)
 */
typedef struct _http_response_info_
{
  char *http_version;
  char *status_code;
  char *reason;
} HTTPResponseInfo;


/* HTTPResponseHeader
 *
 * Struct describing a HTTP response header
 *
 * response_info -> Holds information about the HTTP response
 * fields        -> Key-Value array with all fields of the HTTP response header
 */
typedef struct _http_response_header_
{
  HTTPResponseInfo response_info;
  KeyValueArray fields;
} HTTPResponseHeader;

void initResponseHeader(HTTPResponseHeader *header);
void freeResponseHeader(HTTPResponseHeader *header);

int parseResponseHeader(HTTPResponseHeader *target, const char *buffer);
int parseResponseLine(HTTPResponseInfo *resp_info, const char *buffer);


//#############################
// Regex parsing

int regexParse(const char *regex_string, size_t n_matches, char **parse_targets, const char *buffer);
int parseFields(KeyValueArray *fields, const char *buffer);



#endif
