#include <assert.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "http.h"
#include "util.h"

//#############################
// Response

/* initResponseHeader
 *
 * Initialize a response header struct
 *
 * @param header Struct to initialize
 */
void initResponseHeader(HTTPResponseHeader *header)
{
        assert(header != NULL);

        header->response_info.http_version = NULL;
        header->response_info.status_code = NULL;
        header->response_info.reason = NULL;

        initKeyValueArray(&(header->fields));
}

/* freeResponseHeader
 *
 * Destroy and free a response header struct
 *
 * @param header Struct to free
 */
void freeResponseHeader(HTTPResponseHeader *header)
{
        assert(header != NULL);

        free(header->response_info.http_version);
        header->response_info.http_version = NULL;
        free(header->response_info.status_code);
        header->response_info.status_code = NULL;
        free(header->response_info.reason);
        header->response_info.reason = NULL;
        freeKeyValueArray(&(header->fields));
}

/* parseResponseHeader
 *
 * Parse HTTP response header from provided buffer and store information in response header struct
 *
 * @param header Struct the parsed information should be stored in
 * @param buffer Buffer containing a HTTP response header
 * @ret 0 The HTTP response header has been successfully parsed and stored in the struct
 *      E_NOT_HTTP if the buffer does not contain an empty line
 *      E_NO_MATCH if the buffer does not contain a HTTP response header
 *      E_ALLOC if memory allocation failed
 */
int parseResponseHeader(HTTPResponseHeader *header, const char *buffer)
{
        assert(header != NULL);
        assert(buffer != NULL);

        int retval = 0;

        // Extract header by searching for the first '\r\n\r\n'
        char *header_end = strstr(buffer, "\r\n\r\n");
        if(header_end == NULL)
        {
                retval = E_NOT_HTTP;
                goto error_nl_notfound;
        }

        size_t header_len = header_end + 4 - buffer;
        char *header_string = malloc(header_len + 1);
        if(header_string == NULL)
        {
                retval = E_ALLOC;
                goto error_header_alloc;
        }
        memcpy(header_string, buffer, header_len);
        header_string[header_len] = '\0';

        retval = parseResponseLine(&(header->response_info), header_string);
        if(retval != 0)
        {
                goto e_resp_line_parse_fail;
        }
        retval = parseFields(&(header->fields), header_string);
        if(retval != 0)
        {
                goto e_field_parse_fail;
        }


        // Error handling and cleanup
e_field_parse_fail:
e_resp_line_parse_fail:
        free(header_string);
 error_header_alloc:
 error_nl_notfound:
        return retval;
}


/* parseResponseLine
 *
 * Parse the first line of a HTTP response and store the information in a struct
 *
 * @param resp_info Target struct the parsed information should be stored in
 * @param buffer Buffer containing first line of a HTTP response
 * @ret 0 on success
 *      E_ALLOC when memory allocation failed
 *      E_NO_MATCH when not a HTTP response
 *      E_REG_COMP when regex compilation failed
 */
int parseResponseLine(HTTPResponseInfo *resp_info, const char *buffer)
{
        assert(buffer != NULL);
        assert(resp_info != NULL);

        int retval = 0;
        int ret_stat;

        const char *resp_regex_string = "^HTTP/([^ ]*) ([[:digit:]]*) ([^\r\n]*)";
        const size_t n_resp_matches = 4;
        char *matched_strings[n_resp_matches];
        for(size_t i = 0; i < n_resp_matches; ++i)
        {
                matched_strings[i] = NULL;
        }

        ret_stat = regexParse(resp_regex_string, n_resp_matches, matched_strings, buffer);
        if(ret_stat != 0)
        {
                retval = ret_stat;
                goto end;
        }

        resp_info->http_version = matched_strings[1];
        resp_info->status_code = matched_strings[2];
        resp_info->reason = matched_strings[3];


end:
        return retval;
}




//#############################
// Request

/* initRequestHeader
 *
 * Initialize a request header struct
 *
 * @param header Struct to initialize
 */
void initRequestHeader(HTTPRequestHeader *header)
{
        assert(header != NULL);

        header->request_info.http_version = NULL;
        header->request_info.req_type = NULL;
        header->request_info.resource = NULL;

        initKeyValueArray(&(header->fields));
}

/* freeRequestHeader
 *
 * Destroy and free a request header struct
 *
 * @param header Struct to free
 */
void freeRequestHeader(HTTPRequestHeader *header)
{
        assert(header != NULL);

        free(header->request_info.req_type);
        header->request_info.req_type = NULL;
        free(header->request_info.resource);
        header->request_info.resource = NULL;
        free(header->request_info.http_version);
        header->request_info.http_version = NULL;

        freeKeyValueArray(&(header->fields));
}

/* parseRequestHeader
 *
 * Parse HTTP request header from provided buffer and store information in request header struct
 *
 * @param header Struct the parsed information should be stored in
 * @param buffer Buffer containing a HTTP request header
 * @ret 0 The HTTP request header has been successfully parsed and stored in the struct
 *      E_NOT_HTTP if the buffer does not contain an empty line
 *      E_NO_MATCH if the buffer does not contain a HTTP request header
 *      E_ALLOC if memory allocation failed
 */
int parseRequestHeader(HTTPRequestHeader *header, const char *buffer)
{
        assert(header != NULL);
        assert(buffer != NULL);

        int retval = 0;

        // Extract header by searching for the first '\r\n\r\n'
        char *header_end = strstr(buffer, "\r\n\r\n");
        if(header_end == NULL)
        {
                retval = E_NOT_HTTP;
                goto error_nl_notfound;
        }

        size_t header_len = header_end + 2 - buffer;
        char *header_string = malloc(header_len + 1);
        if(header_string == NULL)
        {
                retval = E_ALLOC;
                goto error_header_alloc;
        }
        memcpy(header_string, buffer, header_len);
        header_string[header_len] = '\0';

        retval = parseRequestLine(&(header->request_info), header_string);
        if(retval != 0)
        {
                goto e_resp_line_parse_fail;
        }
        retval = parseFields(&(header->fields), header_string);
        if(retval != 0)
        {
                goto e_field_parse_fail;
        }


        // Error handling and cleanup
e_field_parse_fail:
e_resp_line_parse_fail:
        free(header_string);
error_header_alloc:
error_nl_notfound:
        return retval;
}


/* parseRquestLine
 *
 * Parse the first line of a HTTP request and store the information in a struct
 *
 * @param resp_info Target struct the parsed information should be stored in
 * @param buffer Buffer containing first line of a HTTP request
 * @ret 0 on success
 *      E_ALLOC when memory allocation failed
 *      E_NO_MATCH when not a HTTP response
 *      E_REG_COMP when regex compilation failed
 */
int parseRequestLine(HTTPRequestInfo *req_info, const char *buffer)
{
        assert(buffer != NULL);
        assert(req_info != NULL);

        int retval = 0;
        int ret_stat;

        const char *req_regex_string = "^([A-Z]*) ([^ ]*) HTTP/([^\r\n]*)";
        const size_t n_req_matches = 4;
        char *matched_strings[n_req_matches];
        for(size_t i = 0; i < n_req_matches; ++i)
        {
                matched_strings[i] = NULL;
        }

        ret_stat = regexParse(req_regex_string, n_req_matches, matched_strings, buffer);
        if(ret_stat != 0)
        {
                retval = ret_stat;
                goto end;
        }

        req_info->req_type = matched_strings[1];
        req_info->resource = matched_strings[2];
        req_info->http_version = matched_strings[3];



end:
        return retval;
}


/* serializeRequestHeader
 *
 * Serialize a request header struct into a string
 *
 * @param request_header Header to serialize
 * @param target_buffer Buffer the string should be written to
 * @param written_length Pointer to variable the length of the written string should be written to
 * @ret 0 on success
 *      -1 when memory allocation failed
 */
int serializeRequestHeader(const HTTPRequestHeader *request_header, char **target_buffer, size_t *written_length)
{
        assert(request_header != NULL);
        assert(target_buffer != NULL);
        assert(written_length != NULL);

        size_t serbuffer_len = requestHeaderLength(request_header);

        char *serbuffer = malloc(serbuffer_len + 1); // +1 to allow for '\0' char
        if(serbuffer == NULL)
        {
                fprintf(stderr, "Buffer for serialization of request header could not be allocated\n");
                return -1;
        }

        serbuffer[0] = '\0';

        strcat(serbuffer, request_header->request_info.req_type);
        strcat(serbuffer, " ");
        strcat(serbuffer, request_header->request_info.resource);
        strcat(serbuffer, " HTTP/");
        strcat(serbuffer, request_header->request_info.http_version);
        strcat(serbuffer, "\r\n");

        for(size_t i = 0; i < request_header->fields.size; ++i)
        {
                strcat(serbuffer, request_header->fields.data[i].key);
                strcat(serbuffer, ": ");
                strcat(serbuffer, request_header->fields.data[i].value);
                strcat(serbuffer, "\r\n");
        }

        strcat(serbuffer, "\r\n");

        *written_length = serbuffer_len;
        *target_buffer = serbuffer;

        return 0;
}


/* requestHeaderLength
 *
 * Get the length of a HTTP request header if it would be serialized
 *
 * @param request_header Header of which the length should be calculated
 * @ret Length of the string if the header was serialized
 */
size_t requestHeaderLength(const HTTPRequestHeader *request_header)
{
        assert(request_header != NULL);

        size_t serbuffer_len = 0;

        serbuffer_len += strlen(request_header->request_info.req_type);
        serbuffer_len += strlen(" ");
        serbuffer_len += strlen(request_header->request_info.resource);
        serbuffer_len += strlen(" HTTP/");
        serbuffer_len += strlen(request_header->request_info.http_version);
        serbuffer_len += strlen("\r\n");

        for(size_t i = 0; i < request_header->fields.size; ++i)
        {
                serbuffer_len += strlen(request_header->fields.data[i].key);
                serbuffer_len += strlen(": ");
                serbuffer_len += strlen(request_header->fields.data[i].value);
                serbuffer_len += strlen("\r\n");
        }

        serbuffer_len += strlen("\r\n");

        return serbuffer_len;
}



//#############################
// Regex

/* regexParse
 *
 * Parse a string using a regex, retrieve matches and subexpressions
 *
 * @param regex_string String containing the regex to search for
 * @param n_matches Number of matches to retrieve. 1 for main expression + subexpressions
 * @param parse_targets Array of strings the matched strings should be stored in
 * @param buffer The buffer to search using the regex
 * @ret 0 on success
 *      E_ALLOC when memory allocation failed
 *      E_REG_COMP when regex compilation failed
 *      E_NO_MATCH when no match was found
 */
int regexParse(const char *regex_string, size_t n_matches, char **parse_targets, const char *buffer)
{
        assert(buffer != NULL);
        assert(parse_targets != NULL);
        assert(regex_string != NULL);

        int retval = 0;
        int ret_stat;

        regex_t regex;

        regmatch_t *matches = malloc(n_matches * sizeof(regmatch_t));
        if(matches == NULL)
        {
                retval = E_ALLOC;
                goto error_matches_alloc;
        }

        // Compile regex
        ret_stat = regcomp(&regex, regex_string, REG_NEWLINE | REG_EXTENDED);
        if(ret_stat != 0)
        {
                fprintf(stderr, "regex compilation failed\n");
                retval = E_REG_COMP;
                goto error_regex_comp;
        }

        // Match regex
        ret_stat = regexec(&regex, buffer, n_matches, matches, 0);
        if(ret_stat != 0)
        {
                retval = E_NO_MATCH;
                goto error_no_match;
        }


        for(size_t i = 0; i < n_matches; ++i)
        {
                const char *str_start = buffer + matches[i].rm_so;
                size_t str_len = matches[i].rm_eo - matches[i].rm_so;

                int strret = setStringN(&(parse_targets[i]), str_start, str_len);
                if(strret != 0)
                {
                        retval = E_ALLOC;
                        for(size_t j = 0; j < i; ++j)
                        {
                                free(parse_targets[i]);
                        }

                        goto error_alloc;
                }

        }

error_alloc:
error_no_match:
        regfree(&regex);
error_regex_comp:
        free(matches);
error_matches_alloc:
        return retval;
}


/* parseFields
 *
 * Parse a series of lines containing one key-value pair per line separated by a colon(:)
 * Store the found key-value pairs in a key-value array
 *
 * @param fields The array to store the found key-value pairs in
 * @param buffer Buffer to search for key-value pairs
 */
int parseFields(KeyValueArray *fields, const char *buffer)
{
        int retval = 0;

        const char *field_regex_string = "^([^:]*): ([^\r\n]*)";
        regex_t field_regex;
        const size_t n_field_matches = 3;
        regmatch_t field_match[n_field_matches];


        //##############
        // Header fields

        // Compile field regex
        int ret = regcomp(&field_regex, field_regex_string, REG_NEWLINE | REG_EXTENDED);
        if(ret != 0)
        {
                fprintf(stderr, "parseFields regex compilation failed\n");
                retval = E_REG_COMP;
                goto error_field_comp;
        }

        // Match field regex
        const char *match_start = buffer;

        while(1)
        {
                int f_matched = regexec(&field_regex, match_start, n_field_matches, field_match, 0);
                if(f_matched != 0)
                {
                        // No more header fields
                        break;
                }


                const char *key_start = match_start + field_match[1].rm_so;
                size_t key_len = field_match[1].rm_eo - field_match[1].rm_so;

                const char *value_start = match_start + field_match[2].rm_so;
                size_t value_len = field_match[2].rm_eo - field_match[2].rm_so;


                addField(fields, key_start, key_len, value_start, value_len);

                // Start next search after the matched field
                match_start += field_match[0].rm_eo;
        }



        // Error handling and cleanup

        regfree(&field_regex);
 error_field_comp:
        return retval;
}
