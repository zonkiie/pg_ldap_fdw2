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
#include <ldap.h>
#include <sasl/sasl.h>

#define _cleanup_cstr_ __attribute((cleanup(free_cstr)))
#define _cleanup_ldap_ __attribute((cleanup(free_ldap)))
#define _cleanup_ldap_message_ __attribute((cleanup(free_ldap_message)))
#define _cleanup_ldap_ber_ __attribute((cleanup(free_ber)))
#define _cleanup_file_ __attribute((cleanup(free_file)))
#define _cleanup_carr_ __attribute((cleanup(free_carr_n)))
#define _cleanup_berval_ __attribute((cleanup(free_berval)))

int str_split(char ***dest, char *str, char *separator);
char * str_replace(const char *str, const char *search, const char *replace);
char *trim(char *string, char *trimchars);
bool char_charlist(char c, char *charlist);
int substr_count(char *str, char *substr);
void free_cstr(char ** str);
void reassign_cstr(char **str, const char * value);
int get_carr_size(char ** carr);
void free_carr_n(char ***carr);
void free_file(FILE** file);
bool in_array(char ** array, char * value);
bool add_to_unique_array(char *** array, char * value);
void free_ldap(LDAP **ldap);
void free_ldap_message(LDAPMessage **message);
void free_ber(BerElement **ber);
void free_berval(struct berval **bval);

#endif
