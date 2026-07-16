
#include "utils.h"
#include "http.h"
#include "stb_ds.h"
#include <asm-generic/errno-base.h>
#include <complex.h>
#include <errno.h>
#include <linux/limits.h>
#include <parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define MAX_BUFFER_SIZE 4096
#define METHOD_MAX_SIZE 32

bool parseLine(char *dst, int writeOffset, char *src, int *readOffset) {
  // return whether the line has terminated
  const char *token = "\r\n";
  int tokenSize = strlen(token);
  int tokenIndex = 0;
  // trying to follow RFC9112

  bool cr = false;
  int i =0;
  while (*readOffset + i < writeOffset) {
    char c = src[*readOffset + i];
    dst[i] = c;
    i++;

    tokenIndex = (c == token[tokenIndex]) ? tokenIndex + 1 : 0;
    if (tokenIndex >= tokenSize) {
      // set to be null terminated
      dst[i- 2] = 0;
      // return with true to indicate that line has been parsed
      *readOffset += i;
      return true;
    }

    if (cr)
      dst[i- 2] = ' ';
    // replace lone CR with space according to RFC

    cr = !cr && c == '\r'; // detect lone carriage return
  }
  *readOffset += i;
  return false;
}

int scanRequest(int connfd, HTTPRequest *request, bool *keepAlive,
                 int *status) {
  Header *headers;
  sh_new_arena(headers);
  char buffer[MAX_BUFFER_SIZE];
  int readoffset = 0;
  int writeoffset = 0;
  int n;
  char currentLine[MAX_BUFFER_SIZE];
  bzero(currentLine, MAX_BUFFER_SIZE);

  do {
    writeoffset += recv(connfd, buffer, MAX_BUFFER_SIZE - writeoffset, 0);
  } while (!parseLine(currentLine, writeoffset, buffer, &readoffset));

  // parse the first line
  char uri[PATH_MAX];
  char method[METHOD_MAX_SIZE];
  char version[METHOD_MAX_SIZE];

  int verMajor = 0;
  int verMinor = 0;
  uint result = sscanf(currentLine, "%31s %4095s HTTP/%d.%d", method, uri,
                       &verMajor, &verMinor);
  if (result != 4) {
    printf("error!");
    *status = HTTP_BAD_REQUEST;
    goto error;
  }
  sprintf(version, "%d.%d", verMajor, verMinor);

  copyString(&request->method, method);
  copyString(&request->uri, uri);
  copyString(&request->version, version);

  bzero(currentLine, MAX_BUFFER_SIZE);
  // keep reading the lines until an empty line is encountered
  while (true) {
    // keep recv until line is constructed
    int tempReadOffset = readoffset;
    while (tempReadOffset < writeoffset &&
           !parseLine(currentLine, writeoffset, buffer, &tempReadOffset)) {
      // if the write offset into the buffer is at the maximum, move all unread
      // data to the start of the buffer;
      if (writeoffset >= MAX_BUFFER_SIZE) {
        int n_chars_left = writeoffset - readoffset;
        memmove(buffer, buffer + readoffset, n_chars_left);
        // zero the right
        bzero(buffer + n_chars_left, MAX_BUFFER_SIZE - n_chars_left);
        writeoffset = n_chars_left;
        tempReadOffset = 0;
        readoffset = 0;
      }
      int nWritten = 0;
      while ((nWritten = recv(connfd, buffer, MAX_BUFFER_SIZE - writeoffset,
                              0)) == -1) {
        if (errno != EINTR) {
          // some other exception;
          *status = HTTP_SERVER_ERROR;
          goto error;
        }
      }
      writeoffset += nWritten;
    }
    int nBytesLine = tempReadOffset - 2 - readoffset; // minus length of CRLF
    if (nBytesLine <= 0) {
      // if the line has 0 bytes we have finished reading the header section
      break;
    }

    readoffset = tempReadOffset;
    // parse current line to a header key value pair
    // allocate a new key val pair

    char key[MAX_BUFFER_SIZE];
    char value[MAX_BUFFER_SIZE];
    sscanf(currentLine, "%4095[^:]: %4095s", key, value);
    shput(headers, key, strdup(value));
    if (!strcmp(key, CONTENT_LENGTH_HEADER_NAME)) {
      request->contentLength = atoi(value);
    }
  }
  //
  char *hasHost = shget(headers, HOST_HEADER_NAME);
  if (!hasHost) {
    *status = HTTP_BAD_REQUEST;
    goto error;
  }
  char *hasContLength = shget(headers, CONTENT_LENGTH_HEADER_NAME);
  char *hasTransferCoding = shget(headers, TRANSFER_CODING_HEADER_NAME);
  if (!strcmp(HTTP_METHOD_GET, request->method.buf) && !hasContLength) {
    // ignore message body if a get request
    goto success;
  } else if (!hasContLength) {
    *status = HTTP_CONTENT_LENGTH_REQUIRED;
    goto error;
    // read message body into request buffer
  } else {
    // read body of message using content length
    // read the rest of the into readoffset
    request->body = malloc(request->contentLength);
    memcpy(request->body, buffer + readoffset, writeoffset - readoffset);
    int nbytesWritten = 0;
    int nBytesLeft = request->contentLength;

    while (((nbytesWritten = recv(connfd, request->body,
                                  request->contentLength - nbytesWritten,
                                  MSG_NOSIGNAL)) == -1) ||
           nbytesWritten < request->contentLength) {
      // if the error code is simply the handling of a signal, then just
      // continue
      bool isEint = errno == EINTR;
      if (nbytesWritten == -1 && !isEint) {
        *status = HTTP_SERVER_ERROR;
        goto error;
      }
      if (!isEint) {
        // if no error code is present, continue receiving data
        nBytesLeft -= nbytesWritten;
      }
    }

    goto success;
  }

success:
  *status = HTTP_OK;
  return 0;
error:
  return -1;
}

int sendResponse(HTTPResponse *response, int connfd) {
  char firstline[MAX_BUFFER_SIZE];

  sprintf(firstline, "HTTP/%s %d %s\r\n", response->version.buf,
          response->statusCode, hmget(code_to_phrase, response->statusCode));

  send(connfd, firstline, strlen(firstline), MSG_NOSIGNAL);
  // allocate buffer for content lenght
  // make sure content-length is present

  char *headersBuffer = NULL;
  encodeHeaders(response->headers, &headersBuffer);
  send(connfd, headersBuffer, strlen(headersBuffer), MSG_NOSIGNAL);

  if (response->body) {
    send(connfd, response->body, response->contentLength, MSG_NOSIGNAL);
  }

  return 0;
}

int encodeHeaders(Header *headers, char **buffer) {
  *buffer = NULL;

  char currentLine[MAX_BUFFER_SIZE * 2 + 20]; //

  for (int i = 0; i < shlen(headers); i++) {
    sprintf(currentLine, "%s: %s\r\n", headers[i].key, headers[i].value);
    int lineLength = strlen(currentLine);
    char *dst = arraddnptr(*buffer, lineLength);
    memcpy(dst, currentLine, lineLength);
  }
  char *dst = arraddnptr(*buffer, 3);
  memcpy(dst, "\r\n", 3); // 3 to incldue \0 at the end

  return 0;
}