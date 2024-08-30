#ifndef __ldapfdwoptions__
#define __ldapfdwoptions__

#include <unistd.h>
#include "LdapFdwTypes.h"

void initLdapFdwOptions(LdapFdwOptions *);
void free_options(LdapFdwOptions *);

#endif
