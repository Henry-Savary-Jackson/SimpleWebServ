#pragma once

#include <utils.h>

#define CONTENT_LENGTH_HEADER_NAME "Content-Length"
#define TRANSFER_CODING_HEADER_NAME "Transfer-Coding"
#define HOST_HEADER_NAME "Host"
#define LOCATION_HEADER_NAME "Location"

// method
#define HTTP_METHOD_GET "GET"
#define HTTP_METHOD_POST "POST"
#define HTTP_METHOD_PUT "PUT"
#define HTTP_METHOD_DELETE "DELETE"

// status code
#define HTTP_OK 200
#define HTTP_NO_CONTENT 204
#define HTTP_MOVED 301
#define HTTP_NOT_MODIFIED 304
#define HTTP_BAD_REQUEST 400
#define HTTP_UNAUTHORIZED 401
#define HTTP_FORBIDDEN 403
#define HTTP_NOT_FOUND 404
#define HTTP_METHOD_UNSUPPORTED 405
#define HTTP_CONTENT_LENGTH_REQUIRED 411
#define HTTP_SERVER_ERROR 500

// status code reason phrase

extern const char *HTTP_OK_PHRASE;
extern const char *HTTP_NO_CONTENT_PHRASE;
extern const char *HTTP_MOVED_PHRASE;

extern const char *HTTP_NOT_MODIFIED_PHRASE;
extern const char *HTTP_BAD_REQUEST_PHRASE;
extern const char *HTTP_UNAUTHORIZED_PHRASE;
extern const char *HTTP_FORBIDDEN_PHRASE;
extern const char *HTTP_NOT_FOUND_PHRASE;
extern const char *HTTP_METHOD_UNSUPPORTED_PHRASE;
extern const char *HTTP_CONTENT_LENGTH_REQUIRED_PHRASE;
extern const char *HTTP_SERVER_ERROR_PHRASE;

typedef struct {
  int key;
  const char *value;
} HttpCodePhrase;

extern HttpCodePhrase *code_to_phrase;

typedef struct {
  char *key;
  char *value;
} Header;

typedef struct {
  Header *headers;
  Header *urlParams;
  String uri;
  String method;
  String version;
  int contentLength;
  char *body;

} HTTPRequest;

typedef struct {
  HTTPRequest *request;
  Header *headers;
  int contentLength;
  int statusCode;
  char *body;
  String version;

} HTTPResponse;

void initHTTPRequest(HTTPRequest *request);
void initHTTPResponse(HTTPResponse *response);
void freeHTTPRequest(HTTPRequest *request);
void freeHTTPResponse(HTTPResponse *response);
void setRequest(HTTPResponse *resp, HTTPRequest *resq);

void setResponseBody(HTTPResponse *response, char *buffer, int contentLength) ;
void init_code_to_phrase();