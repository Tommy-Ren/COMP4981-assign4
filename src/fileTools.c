//
// Created by Kiet & Tommy on 12/1/25.
//

#include "../include/fileTools.h"
#include "../include/stringTools.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct fileData
{
    // The number of chars in the file name.
    int fileNameLength;

    // The file name.
    char *fileName;

    // The number of chars in the file.
    long contentLength;

    // The content of the file as a string.
    char *content;
};

struct fileData *initializeFileDataStruct(int fileNameLength, const char *fileName, long contentLength, const char *content)
{
    struct fileData *fileData;

    // If invalid fileNameLength, exit with error.
    if(fileNameLength <= 0)
    {
        const char errorMSG[] = "Invalid fileNameLength: %d\n";
        fprintf(stderr, errorMSG, fileNameLength);
        exit(EXIT_FAILURE);
    }

    // If invalid contentLength, exit with error.
    if(contentLength < 0)
    {
        const char errorMSG[] = "Invalid contentLength: %ld\n";
        fprintf(stderr, errorMSG, contentLength);
        exit(EXIT_FAILURE);
    }

    // If !fileName, exit with error.
    if(!fileName)
    {
        fprintf(stderr, "!fileName\n");
        exit(EXIT_FAILURE);
    }

    // If !content, exit with error.
    if(!content)
    {
        perror("!content\n");
        exit(EXIT_FAILURE);
    }

    // Allocate the memory.
    fileData = (struct fileData *)malloc(sizeof(struct fileData));

    // If the memory failed to allocate, exit with an error.
    if(!fileData)
    {
        perror("Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    // Insert the ints.
    fileData->fileNameLength = fileNameLength;
    fileData->contentLength  = contentLength;

    // Copy the strings.
    fileData->fileName = strdup(fileName);
    fileData->content  = strdup(content);

    return fileData;
}

struct fileData *getFileDataFromFilePath(const char *filePath)
{
    struct fileData *fileData;
    int              fd;    // File descriptor

    // File data params
    int         fileNameLength;
    long        contentLength;
    const char *fileName;
    char       *content = NULL;

    // If !filePath, exit with error.
    if(!filePath)
    {
        perror("!filePath\n");
        exit(EXIT_FAILURE);
    }

    // Open the file.
    fd = open(filePath, O_RDONLY | O_CLOEXEC);    // Open file in read-only mode with O_CLOEXEC
    if(fd == -1)
    {
        perror("Error opening file.");
        exit(EXIT_FAILURE);
    }

    // Get the file size using lseek
    contentLength = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);    // Reset file pointer

    // If the content length is 0, we don't allocate memory
    if(contentLength > 0)
    {
        // Allocate memory for the content.
        content = (char *)malloc((size_t)(contentLength + 1));
        if(!content)
        {
            perror("Memory allocation failed.");
            exit(EXIT_FAILURE);
        }

        // Read the content
        read(fd, content, (size_t)contentLength);
        content[contentLength] = '\0';    // Null-terminate the string
    }

    // Get the file name
    fileName       = getLastToken(filePath, "/").token;
    fileNameLength = (int)strlen(fileName);

    // Initialize fileData struct, handle case where content is NULL (empty file)
    fileData = initializeFileDataStruct(fileNameLength, fileName, contentLength, content);

    // Close the file descriptor
    close(fd);

    return fileData;
}

void printFileDataStruct(const struct fileData *fileData)
{
    printf("struct fileData: {"
           "\n\tfileNameLength: %d"
           "\n\tfileName: %s"
           "\n\tcontentLength: %ld"
           "\n\tcontent: %s\n}",
           fileData->fileNameLength,
           fileData->fileName,
           fileData->contentLength,
           fileData->content);
}

void appendTextToFile(const char *filePath, const char *text)
{
    FILE       *file;
    const char *mode = "a";    // Append mode

    // Open the file in append mode
    file = fopen(filePath, mode);
    if(file == NULL)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Append the text to the file
    if(fputs(text, file) == EOF)
    {
        perror("Error writing to file");
        fclose(file);    // Make sure to close the file even if writing fails
        exit(EXIT_FAILURE);
    }

    // Close the file
    if(fclose(file) == EOF)
    {
        perror("Error closing file");
        exit(EXIT_FAILURE);
    }
}

int writeTextToFile(const char *filePath, const char *content)
{
    FILE *file = fopen(filePath, "we");
    if(file == NULL)
    {
        perror("Error opening file");
        return -1;
    }

    if(fputs(content, file) == EOF)
    {
        perror("Error writing to file");
        fclose(file);
        return -1;
    }

    if(fclose(file) == EOF)
    {
        perror("Error closing file");
        return -1;
    }

    return 0;
}
