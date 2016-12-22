#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "util.h"

/* setString
 *
 * Set the destination to contain the source string.
 * Allocates memory to hold the string and frees previous contents of destination string.
 * Source string must be null terminated.
 *
 * @param destination Pointer to char * (=string) that should be modified to hold the source string
 * @param destination Source string that should be copied to the destination
 * @ret 0 on success
 *      -1 if memory could not be allocated, the destination char pointer remains unmodified
 */
int setString(char **destination, const char *source)
{
        return setStringN(destination, source, strlen(source));
}

/* setStringN
 *
 * Set the destination to contain the first N characters of the source string.
 * Allocates memory to hold the string and frees previous contents of destination string.
 *
 * @param destination Pointer to char * (=string) that should be modified to hold the source string
 * @param destination Source string that should be copied to the destination
 * @param source_len Length of the string to be copied
 * @ret 0 on success
 *      -1 if memory could not be allocated, the destination char pointer remains unmodified
 */
int setStringN(char **destination, const char *source, size_t source_len)
{
        assert(source != NULL);
        assert(destination != NULL);

        char *str = malloc(source_len + 1);
        if(str == NULL)
        {
                return -1;
        }

        memcpy(str, source, source_len);
        str[source_len] = '\0';


        // Delete previous content
        if(*destination != NULL)
        {
                free(*destination);
        }

        *destination = str;

        return 0;
}


/* initKeyValueArray
 *
 * Initialize a key-value array
 *
 * @param array The array to initialize
 */
void initKeyValueArray(KeyValueArray *array)
{
        assert(array != NULL);

        // Init KeyValue array
        array->data = NULL;
        array->size = 0;
}

/* initKeyValue
 *
 * Initialize a key-value element
 *
 * @param element The key-value element to initialize
 */
void initKeyValue(KeyValue *element)
{
        assert(element != NULL);

        element->key = NULL;
        element->key_length = 0;
        element->value = NULL;
        element->value_length = 0;
}


/* freeKeyValueArray
 *
 * Free and destroy a key-value array
 *
 * @param array The array to free
 */
void freeKeyValueArray(KeyValueArray *array)
{
        assert(array != NULL);

        if(array->size != 0)
        {
                for(size_t i = 0; i < array->size; ++i)
                {
                        free(array->data[i].key);
                        free(array->data[i].value);
                }

                free(array->data);
                array->data = NULL;
                array->size = 0;
        }
}


/* addField
 *
 * Add a key-value pair to the array
 *
 * @param array The array the element should be added to
 * @param key The key string
 * @param keylen Length of the key string
 * @param value The value string corresponding to the key
 * @param valuelen Length of the value string
 * @ret 0 if element has been successfully inserted into the array
 *      -1 if allocation failed
 */
int addField(KeyValueArray *array, const char *key, size_t keylen, const char *value, size_t valuelen)
{
        assert(array != NULL);

        int retval = 0;


        if(array->size == 0)
        {
                array->data = malloc(sizeof(KeyValue));
                if(array->data == NULL)
                {
                        retval = -1;
                        goto error_alloc;
                }

                initKeyValue(array->data);
        }
        else
        {
                KeyValue *temp = realloc(array->data, (array->size + 1) * sizeof(KeyValue));
                if(temp == NULL)
                {
                        retval = -1;
                        goto error_alloc;
                }

                array->data = temp;

                initKeyValue(array->data + array->size);
        }


        KeyValue element;
        initKeyValue(&element);

        if(setStringN(&(element.key), key , keylen) != 0)
        {
                retval = -1;
                goto error_key;
        }
        element.key_length = keylen;

        if(setStringN(&(element.value), value, valuelen) != 0)
        {
                retval = -1;
                free(element.key);
                goto error_value;
        }
        element.value_length = valuelen;

        array->data[array->size] = element;
        array->size += 1;

        //#########################
        // Error handling

error_value:
error_key:
error_alloc:
        return retval;
}

/* getValue
 *
 * Retrieve the value string associated with the given key from the key-value array
 *
 * @param array The array to search in
 * @param key Key to search for
 * @ret const char pointer to value string or NULL if not found
 */
const char * getValue(const KeyValueArray *array, const char *key)
{
        assert(array != NULL);
        assert(key != NULL);

        for(size_t i = 0; i < array->size; ++i)
        {
                if(strcmp(array->data[i].key, key) == 0)
                {
                        // Key-value pair found

                        return array->data[i].value;
                }
        }

        return NULL;
}


/* setValue
 *
 * Change the value associated with the given key
 *
 * @param array Key-Value array that should be modified
 * @param key Key to search for
 * @param new_value New value string that should be associated with the key
 * @param new_value_len Length of the new value string
 * @ret 0 on success
 *      -1 if the key could not be found or failed to allocate memory
 */
int setValue(KeyValueArray *array, const char *key, const char *new_value, size_t new_value_len)
{
        assert(array != NULL);
        assert(key != NULL);

        for(size_t i = 0; i < array->size; ++i)
        {
                if(strcmp(array->data[i].key, key) == 0)
                {
                        // Key-value pair found
                        
                        char *temp = malloc(new_value_len + 1);
                        if(temp == NULL)
                        {
                                fprintf(stderr, "Failed to allocate memory for value\n");
                                return -1;
                        }
                        memcpy(temp, new_value, new_value_len);
                        temp[new_value_len] = '\0';

                        free(array->data[i].value);
                        array->data[i].value = temp;
                        array->data[i].value_length = new_value_len;

                        return 0;
                }
        }

        return -1;
}
