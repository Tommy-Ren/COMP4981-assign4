//
// Created by tom on 4/5/2025.
//

#include <fcntl.h>
#include <ndbm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 

void print_db_entries(const char *db_path)
{
    DBM *db = dbm_open(db_path, O_RDONLY, 0);
    if(!db)
    {
        perror("Failed to open DB");
        exit(EXIT_FAILURE);
    }

    printf("=== Contents of %s ===\n", db_path);

    datum key = dbm_firstkey(db);
    while(key.dptr != NULL)
    {
        datum value = dbm_fetch(db, key);
        if(value.dptr != NULL)
        {
            printf("Key: %.*s\n", key.dsize, key.dptr);
            printf("Value: %.*s\n", value.dsize, value.dptr);
            printf("----\n");
        }

        key = dbm_nextkey(db);
    }

    dbm_close(db);
}

int main(int argc, char *argv[])
{
    const char *db_path = "../data/db/post_data.db";    // default

    if(argc == 2)
    {
        db_path = argv[1];
    }

    print_db_entries(db_path);

    return 0;
}
