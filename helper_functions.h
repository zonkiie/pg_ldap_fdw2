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

int str_split(char ***, char *, char *);
char * str_replace(const char *, const char *, const char *);
char *trim(char *, char *);
bool char_charlist(char , char *);
int substr_count(char *, char *);
void free_cstr(char ** );
void reassign_cstr(char **, const char * );
int get_carr_size(char ** );
void free_carr_n(char ***);
void free_file(FILE** );
bool in_array(char ** , char * );
bool add_to_unique_array(char *** , char * );
void free_ldap(LDAP **);
void free_ldap_message(LDAPMessage **);
void free_ber(BerElement **);
void free_berval(struct berval **);

#endif
