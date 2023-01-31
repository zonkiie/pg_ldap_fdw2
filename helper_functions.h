#ifndef __helper_functions__
#define __helper_functions__

#define _cleanup_cstr_ __attribute((cleanup(free_cstr)))
#define _cleanup_ldap_ __attribute((cleanup(free_ldap)))
#define _cleanup_ldap_message_ __attribute((cleanup(free_ldap_message)))
#define _cleanup_ldap_ber_ __attribute((cleanup(free_ber)))
#define _cleanup_file_ __attribute((cleanup(free_file)))
#define _cleanup_carr_ __attribute((cleanup(free_carr_n)))
#define _cleanup_berval_ __attribute((cleanup(free_berval)))

#endif
