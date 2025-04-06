/*******************************************************************************
 * User Database Management System
 *
 * This module provides a persistent storage interface for user management using
 * DBM (Database Manager) for data storage. It supports both account credential
 * operations (for login/creation) and user list management. All user data is
 * stored persistently via DBM, so no in-memory global array is needed.
 ******************************************************************************/

#include "../include/db.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#pragma GCC diagnostic ignored "-Waggregate-return"

#define MAX_KEY 64

/* Opens the DBM database specified by dbo->name.
   Returns 0 on success, -1 on error. */
ssize_t database_open(DBO *dbo)
{
    dbo->db = dbm_open(dbo->name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if(!dbo->db)
    {
        perror("dbm_open failed");
        return -1;
    }
    return 0;
}

/* Stores the string value under the given key in the database.
   Returns 0 on success, -1 on failure. */
int store_string(DBM *db, const char *key, const char *value)
{
    const_datum key_datum   = MAKE_CONST_DATUM(key);
    const_datum value_datum = MAKE_CONST_DATUM(value);

    return dbm_store(db, *(datum *)&key_datum, *(datum *)&value_datum, DBM_REPLACE);
}

int store_int(DBM *db, const char *key, int value)
{
    const_datum key_datum = MAKE_CONST_DATUM(key);
    datum       value_datum;
    int         result;

    value_datum.dptr = (char *)malloc(TO_SIZE_T(sizeof(int)));

    if(value_datum.dptr == NULL)
    {
        return -1;
    }

    memcpy(value_datum.dptr, &value, sizeof(int));
    value_datum.dsize = sizeof(int);

    result = dbm_store(db, *(datum *)&key_datum, value_datum, DBM_REPLACE);

    free(value_datum.dptr);
    return result;
}

int store_post_entry(DBO *dbo, const char *body_string, const char *pk_name)
{
    int  current_id;
    char key[MAX_KEY];

    // Open the database if not already open
    if(database_open(dbo) < 0)
    {
        return -1;
    }

    // Get the current primary key
    if(retrieve_int(dbo->db, pk_name, &current_id) < 0)
    {
        // If not found, initialize it
        current_id = 0;
    }

    // Format the key like "entry_0001"

    snprintf(key, sizeof(key), "entry_%04d", current_id);

    // Store the full body string under the generated key
    if(store_string(dbo->db, key, body_string) != 0)
    {
        fprintf(stderr, "Failed to store POST entry in DB\n");
        dbm_close(dbo->db);
        return -1;
    }

    // Increment the primary key and save it back
    current_id++;
    if(store_int(dbo->db, pk_name, current_id) != 0)
    {
        fprintf(stderr, "Failed to update primary key in DB\n");
        dbm_close(dbo->db);
        return -1;
    }

    dbm_close(dbo->db);
    return 0;
}

int retrieve_int(DBM *db, const char *key, int *result)
{
    datum       fetched;
    const_datum key_datum = MAKE_CONST_DATUM(key);

    fetched = dbm_fetch(db, *(datum *)&key_datum);

    if(fetched.dptr == NULL || fetched.dsize != sizeof(int))
    {
        return -1;
    }

    memcpy(result, fetched.dptr, sizeof(int));

    return 0;
}

void *retrieve_byte(DBM *db, const void *key, size_t size)
{
    const_datum key_datum;
    datum       result;
    char       *retrieved_str;

    key_datum = MAKE_CONST_DATUM_BYTE(key, size);

    result = dbm_fetch(db, *(datum *)&key_datum);

    if(result.dptr == NULL)
    {
        return NULL;
    }

    retrieved_str = (char *)malloc(TO_SIZE_T(result.dsize));

    if(!retrieved_str)
    {
        return NULL;
    }

    memcpy(retrieved_str, result.dptr, TO_SIZE_T(result.dsize));

    return retrieved_str;
}
