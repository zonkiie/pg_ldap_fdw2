# pg_ldap_fdw2
LDAP Foreign Data Wrapper for PostgreSQL.  
I will try to implement a Read AND Write FDW.

## Usage
    CREATE EXTENSION ldap2_fdw;
    CREATE SERVER ldap FOREIGN DATA WRAPPER ldap2_fdw OPTIONS (uri 'ldap://localhost', username 'cn=admin', password '<admin password>');
    -- User Mapping will be implemented later
    CREATE FOREIGN TABLE IF NOT EXISTS names (id uuid NOT NULL, cn varchar NOT NULL, sn varchar NOTNULL) SERVER ldap OPTIONS(uri 'ldap://localhost', username 'cn=admin', password 'password');
    DROP FOREIGN TABLE IF EXISTS names SERVER ldap;
    DROP SERVER ldap;
    DROP EXTENSION ldap2_fdw;
    

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

## Error constants
[https://github.com/munakoiso/logerrors/blob/master/constants.h](https://github.com/munakoiso/logerrors/blob/master/constants.h)
