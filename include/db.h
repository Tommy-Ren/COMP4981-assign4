#ifndef DB_H
#define DB_H
#include <inttypes.h>
#include <string.h>    // for strlen

#ifdef __APPLE__
    #include <ndbm.h>
typedef size_t datum_size;
#elif defined(__FreeBSD__)
    #include <gdbm.h>
typedef int datum_size;
#else
    /* Assume Linux/Ubuntu; using gdbm_compat which provides an ndbm-like interface */
    #include <ndbm.h>
typedef int datum_size;
#endif

#define TO_SIZE_T(x) ((size_t)(x))

typedef struct
{
    // cppcheck-suppress unusedStructMember
    const void *dptr;
    // cppcheck-suppress unusedStructMember
    datum_size dsize;
} const_datum;

#define MAKE_CONST_DATUM(str) ((const_datum){(str), (datum_size)strlen(str) + 1})
#define MAKE_CONST_DATUM_BYTE(str, size) ((const_datum){(str), (datum_size)(size)})

typedef struct DBO
{
    char *name;    // cppcheck-suppress unusedStructMember
    DBM  *db;      // cppcheck-suppress unusedStructMember
} DBO;

/* Opens the database specified in dbo->name in read/write mode (creating it if needed).
   Returns 0 on success, -1 on failure. */
ssize_t database_open(DBO *dbo);

/* Stores the string value under the key into the given DBM.
   Returns 0 on success, -1 on failure. */
int store_string(DBM *db, const char *key, const char *value);

/**
 * @brief Stores a parsed POST key-value entry in the DB using a unique entry key.
 * The entry key will be generated like "entry_001", "entry_002", etc.
 * @param dbo The database object.
 * @param body_string The raw POST body (e.g., "name=Mi&email=mi@example.com").
 * @param pk_name The primary key counter name (e.g., "entry_id").
 * @return 0 on success, -1 on failure.
 */
int store_post_entry(DBO *dbo, const char *body_string, const char *pk_name);

/* Retrieves an integer from the DBM.
   Returns 0 on success, -1 on failure. */
int retrieve_int(DBM *db, const char *key, int *result);

/* Retrieves raw bytes from the DBM.
   Returns pointer on success, or NULL if not found. */
void *retrieve_byte(DBM *db, const void *key, size_t size);

#endif    // DB_H
