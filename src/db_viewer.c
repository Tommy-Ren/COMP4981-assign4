//
// Created by tom on 4/5/2025.
//

#include <fcntl.h>
#include <ndbm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_db_entries(const char *db_path);

void print_db_entries(const char *db_path)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"    // aggregate-return
    DBM *db = dbm_open((char *)db_path, O_RDONLY, 0);
#pragma GCC diagnostic pop
    datum key;

    if(!db)
    {
        perror("Failed to open DB");
        exit(EXIT_FAILURE);
    }

    printf("=== Contents of %s ===\n", db_path);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waggregate-return"
    key = dbm_firstkey(db);
#pragma GCC diagnostic pop

    while(key.dptr != NULL)
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waggregate-return"
        datum value = dbm_fetch(db, key);
#pragma GCC diagnostic pop

        if(value.dptr != NULL)
        {
            printf("Key: %.*s\n", key.dsize, key.dptr);
            printf("Value: %.*s\n", value.dsize, value.dptr);
            printf("----\n");
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waggregate-return"
        key = dbm_nextkey(db);
#pragma GCC diagnostic pop
    }

    dbm_close(db);
}

int main(int argc, const char *argv[])
{
    const char *db_path = "../data/db/post_data.db";    // default

    if(argc == 2)
    {
        db_path = argv[1];
    }

    print_db_entries(db_path);

    return 0;
}
