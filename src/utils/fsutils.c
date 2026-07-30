#include <stdio.h>
#include <errno.h>

int writeToFile(FILE *file, char *chunk, int size)
{
    size_t totalWritten = 0;
    size_t nwritten = 0;
    while (totalWritten < size)
    {
        nwritten = fwrite(chunk + totalWritten, 1, size - totalWritten, file);
        if (ferror(file) && errno == EINTR)
        {
            clearerr(file);
        }
        if (ferror(file))
        {
            return -1;
        }
        totalWritten += nwritten;
    }
    return 0;
}

int readFromFile(FILE *file, char *chunk, int size)
{
    size_t totalRead = 0;
    size_t numRead = 0;
    while (totalRead < size)
    {
        numRead = fread(chunk + totalRead, 1, size - totalRead, file);
        if (feof(file))
        {
            return (int)(totalRead + numRead);
        }
        if (ferror(file) && errno == EINTR)
        {
            clearerr(file);
        }
        else if (ferror(file))
        {
            return -1;
        }
        totalRead += numRead;
    }
    return (int)totalRead;
}
