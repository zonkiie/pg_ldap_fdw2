#ifndef __ldap_types__
#define __ldap_types__

typedef struct {
    char * ldap_type_name;
    char * ldap_type_oid;
    char * postgres_type;
} ldap_type;

/**
 * @see https://www.zytrax.com/books/ldap/apa/types.html
 * needs to be extended and fixed
 */
ldap_type ldap_type_map[] = {
    {"IA5String", "1.3.6.1.4.1.1466.115.121.1.26", "varchar"},
    {"DirectoryString", "1.3.6.1.4.1.1466.115.121.1.15", "varchar"},
    {"PrintableString", "1.3.6.1.4.1.1466.115.121.1.44", "varchar"},
    {"OctetString", "1.3.6.1.4.1.1466.115.121.1.40", "varchar"},
    {"PostalAddress", "1.3.6.1.4.1.1466.115.121.1.41", "varchar"},
    {"CountryString", "1.3.6.1.4.1.1466.115.121.1.11", "varchar"},
    {"NumericString", "1.3.6.1.4.1.1466.115.121.1.36", "varchar"},
    {"Integer", "1.3.6.1.4.1.1466.115.121.1.27", "integer"},
    {"GeneralizedTime", "1.3.6.1.4.1.1466.115.121.1.24", "timestamp"},
    {"TelephoneNumber", "1.3.6.1.4.1.1466.115.121.1.50", "varchar"},
    {"Boolean", "1.3.6.1.4.1.1466.115.121.1.7", "boolean"},
    {"Binary", "1.3.6.1.4.1.1466.115.121.1.5", "bytea"},
    {"DN", "1.3.6.1.4.1.1466.115.121.1.12", "varchar"},
    {"BitString", "1.3.6.1.4.1.1466.115.121.1.6", "varchar"},
    {NULL, NULL, NULL},
};

#endif
