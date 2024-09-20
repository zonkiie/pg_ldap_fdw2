# pg_ldap_fdw2
LDAP Foreign Data Wrapper for PostgreSQL with write support.
Warning: Don't use it in production systems. This fdw is not enough tested and still needs a lot of work.

## TODO
Memory management, returning written/deleted rows, code reworking, error handling, write documentation...

## Usage (a better instruction guide will follow)
    - Set up your test system
    - install postgresql and openldap dev packages
    - setup postgresql and ldap server
    - git clone this repo
    - cd <cloned dir>
    - make install

## Install
    DROP EXTENSION IF EXISTS ldap2_fdw CASCADE;
    CREATE EXTENSION IF NOT EXISTS ldap2_fdw;
    CREATE SERVER IF NOT EXISTS ldap FOREIGN DATA WRAPPER ldap2_fdw OPTIONS (uri 'ldap://localhost');
    CREATE USER MAPPING IF NOT EXISTS FOR CURRENT_USER SERVER ldap OPTIONS(username 'cn=admin,dc=nodomain', password 'password');
    CREATE FOREIGN TABLE IF NOT EXISTS names (id uuid NOT NULL, dn varchar NOT NULL, cn varchar NOT NULL, sn varchar NOT NULL) SERVER ldap OPTIONS(basedn 'dc=nodomain', filter '(objectclass=*)', scope 'LDAP_SCOPE_CHILDREN');
## Remote Schema Import
    IMPORT FOREIGN SCHEMA "dc=nodomain" FROM SERVER ldap INTO public OPTIONS(basedn 'dc=nodomain', objectclass 'person', objectclass 'inetOrgPerson', schemadn 'cn=subschema', tablename 'names', scope 'LDAP_SCOPE_CHILDREN', filter '(objectClass=*)');
## Uninstall
    DROP FOREIGN TABLE IF EXISTS names;
    DROP SERVER IF EXISTS ldap CASCADE;
    DROP EXTENSION IF EXISTS ldap2_fdw CASCADE;

## Usage
    

## A list of Foreign Data Wrappers
[https://wiki.postgresql.org/wiki/Foreign_data_wrappers](https://wiki.postgresql.org/wiki/Foreign_data_wrappers)

## Sources
[https://github.com/wikrsh/hello_fdw](https://github.com/wikrsh/hello_fdw)

[https://github.com/slaught/dummy_fdw](https://github.com/slaught/dummy_fdw)

[https://github.com/adunstan/file_text_array_fdw](https://github.com/adunstan/file_text_array_fdw)

[https://github.com/guedes/ldap_fdw](https://github.com/guedes/ldap_fdw)

[https://github.com/EnterpriseDB/mongo_fdw](https://github.com/EnterpriseDB/mongo_fdw)

[https://www.dolthub.com/blog/2022-01-26-creating-a-postgres-foreign-data-wrapper/](https://www.dolthub.com/blog/2022-01-26-creating-a-postgres-foreign-data-wrapper/)

[https://www.ibm.com/docs/en/zos/2.4.0?topic=schema-retrieving](https://www.ibm.com/docs/en/zos/2.4.0?topic=schema-retrieving)

[https://wiki.mozilla.org/Mozilla_LDAP_SDK_Programmer%27s_Guide/Adding,_Updating,_and_Deleting_Entries_With_LDAP_C_SDK](https://wiki.mozilla.org/Mozilla_LDAP_SDK_Programmer%27s_Guide/Adding,_Updating,_and_Deleting_Entries_With_LDAP_C_SDK)

## Error constants
[https://github.com/munakoiso/logerrors/blob/master/constants.h](https://github.com/munakoiso/logerrors/blob/master/constants.h)

[https://www.openldap.com/lists/openldap-devel/200108/msg00006.html](https://www.openldap.com/lists/openldap-devel/200108/msg00006.html)

## License
will be decided in the near future
provisionally GPLv3

# dump ldap packages
    tcpdump -i lo -q -A port 389

