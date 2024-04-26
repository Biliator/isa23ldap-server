#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <regex.h>
#include <signal.h>
#include <unistd.h>
#include <wait.h>

#define MAX_LINE_LENGTH 256
#define MAX_FIELDS 3
#define _GNU_SOURCE

volatile sig_atomic_t ctrl_c_pressed = 0;

typedef struct
{
    char cn[MAX_LINE_LENGTH];
    char uid[MAX_LINE_LENGTH];
    char mail[MAX_LINE_LENGTH];
} database;

typedef struct attributes
{
    char *cn;
    char *uid;
    char *mail;
    struct attributes *next;
} attributes;

attributes *create_attributes(char *cn, char *uid, char *mail);
void append_attributes(attributes **head, attributes *new_attributes);
void free_attributes_list(attributes *head);
void search_database(char *filename, char *attribute_desc, char *assertion_value, 
                        attributes **res_content, int filter);
void get_message(char *buff, int socket, char *file_name);
int process_filter(char *filter, char **result, char **attribute_desc, char **assertion_value);
void concat_hex_arrays(char *array1, char *array2, char **result);
int int_to_string(int number, char *array, char **result) ;
int send_response(attributes *head, char *attribute_description, 
                    int attributes_length, char *base_object, 
                    char *proccesed_filter, int socket, int size_limit);
void tcp(int port, char *file);
void setup_signal_handlers();
void sigint_handler() ;