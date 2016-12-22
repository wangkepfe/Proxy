#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

/* KeyValue struct
 *
 * Struct holding a key and value string and the corresponding lengths
 *
 * key          -> Key string
 * key_length   -> Length of the key string
 * value        -> Value string corresponding to the keylen
 * value_length -> Length of the value string
 *
 */
typedef struct _key_value_
{
  char *key;
  size_t key_length;
  char* value;
  size_t value_length;
} KeyValue;


/* KeyValueArray
 *
 * Array for storing key-value pairs. Keeps track of the size and provides
 * an interface to interact with the array
 *
 * data -> array (pointer) the key-value pairs are stored in
 * size -> size of the array
 *
 */
typedef struct _key_value_array_
{
  KeyValue *data;
  size_t size;
} KeyValueArray;

void initKeyValue(KeyValue *element);

void initKeyValueArray(KeyValueArray *array);
void freeKeyValueArray(KeyValueArray *array);

int addField(KeyValueArray *array, const char *key, size_t keylen, const char *value, size_t valuelen);
const char * getValue(const KeyValueArray *array, const char *key);
int setValue(KeyValueArray *array, const char *key, const char *new_value, size_t new_value_len);


int setString(char **destination, const char *source);
int setStringN(char **destination, const char *source, size_t source_len);

#endif
