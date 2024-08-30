#ifndef __ldapfdwoptions__
#define __ldapfdwoptions__

#include <unistd.h>
#include "LdapFdwTypes.h"

LdapFdwOptions * createLdapFdwOptions();
void initLdapFdwOptions(LdapFdwOptions *);
void free_options(LdapFdwOptions *);

#endif
