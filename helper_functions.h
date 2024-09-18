#ifndef __helper_functions__
#define __helper_functions__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <malloc.h>
#include <search.h>
#include <stddef.h>
#include "LdapFdwOptions.h"
//#include "postgres.h"

extern struct         timeval  zerotime;

#define _cleanup_cstr_ __attribute((cleanup(free_cstr)))
#define _cleanup_file_ __attribute((cleanup(free_file)))
#define _cleanup_carr_ __attribute((cleanup(free_carr_n)))
#define _cleanup_pstr_ __attribute((cleanup(free_pstr)))
#define _cleanup_options_ __attribute((cleanup(free_options)))


int str_split(char ***, char *, char *);
int str_join(char **, char **, char *);
char * str_replace(const char *, const char *, const char *);
char *trim(char *, char *);
bool char_charlist(char , char *);
int substr_count(char *, char *);
char ** array_copy(char **);
void free_cstr(char ** );
void free_pstr(char ** );
void reassign_cstr(char **, const char * );
int get_carr_size(char ** );
void free_carr_n(char ***);
void free_file(FILE** );
bool in_array(char ** , char * );
bool add_to_unique_array(char *** , char * );
size_t array_count(char **);
size_t array_push(char ***, char *);

size_t put_file_content_nm(char*, char*, size_t, char*);
size_t put_file_content_n(char*, char*, size_t);
size_t put_file_content(char*, char*);
size_t append_file_content_n(char*, char*, size_t);
size_t append_file_content(char*, char*);

#endif
