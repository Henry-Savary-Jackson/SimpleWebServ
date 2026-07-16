#include <http.h>
#include <stb_ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utils.h>

const char *HTTP_OK_PHRASE = "OK";
const char *HTTP_NO_CONTENT_PHRASE = "No Content";
const char *HTTP_MOVED_PHRASE = "Moved Permanently";
const char *HTTP_NOT_MODIFIED_PHRASE = "Not Modified";
const char *HTTP_BAD_REQUEST_PHRASE = "Bad Request";
const char *HTTP_UNAUTHORIZED_PHRASE = "Unauthorized";
const char *HTTP_FORBIDDEN_PHRASE = "Forbidden";
const char *HTTP_NOT_FOUND_PHRASE = "Not Found";
const char *HTTP_METHOD_UNSUPPORTED_PHRASE = "Method Not Allowed";
const char *HTTP_CONTENT_LENGTH_REQUIRED_PHRASE = "Length Required";
const char *HTTP_SERVER_ERROR_PHRASE = "Internal Server Error";

HttpCodePhrase *code_to_phrase = NULL;

void initHTTPRequest(HTTPRequest *request) {
  initString(&request->method);
  initString(&request->uri);
  initString(&request->version);
  request->contentLength = 0;
  request->body = NULL;
  sh_new_arena(request->headers);
  sh_new_arena(request->urlParams);
}

void freeHTTPRequest(HTTPRequest *request) {
  freeString(&request->method);
  freeString(&request->uri);
  freeString(&request->version);
  if (request->body)
    free(request->body);
  shfree(request->headers);
  shfree(request->urlParams);
}

void initHTTPResponse(HTTPResponse *response) {
  response->contentLength = 0;
  response->statusCode = 0;
  response->body = NULL;
  sh_new_arena(response->headers);
  initString(&response->version);
  response->request = NULL;
}

void setResponseBody(HTTPResponse *response, char *buffer, int contentLength) {
  response->contentLength = contentLength;
  if (response->body)
    free(response->body);
  response->body = malloc(contentLength);
  memcpy(response->body, buffer, contentLength);
  char contentLengthS[32];
  sprintf(contentLengthS, "%d", contentLength);
  shput(response->headers, CONTENT_LENGTH_HEADER_NAME, strdup(contentLengthS));
}

void freeHTTPResponse(HTTPResponse *response) {
  if (response->body)
    free(response->body);
  freeString(&response->version);
  shfree(response->headers);
}
void setRequest(HTTPResponse *resp, HTTPRequest *resq) {
  copyString(&resp->version, resq->version.buf);
  resp->request = resq;
}

void init_code_to_phrase() {
  hmput(code_to_phrase, HTTP_OK, HTTP_OK_PHRASE);
  hmput(code_to_phrase, HTTP_NO_CONTENT, HTTP_NO_CONTENT_PHRASE);
  hmput(code_to_phrase, HTTP_MOVED, HTTP_MOVED_PHRASE);
  hmput(code_to_phrase, HTTP_NOT_MODIFIED, HTTP_NOT_MODIFIED_PHRASE);
  hmput(code_to_phrase, HTTP_BAD_REQUEST, HTTP_BAD_REQUEST_PHRASE);
  hmput(code_to_phrase, HTTP_UNAUTHORIZED, HTTP_UNAUTHORIZED_PHRASE);
  hmput(code_to_phrase, HTTP_FORBIDDEN, HTTP_FORBIDDEN_PHRASE);
  hmput(code_to_phrase, HTTP_NOT_FOUND, HTTP_NOT_FOUND_PHRASE);
  hmput(code_to_phrase, HTTP_METHOD_UNSUPPORTED,
        HTTP_METHOD_UNSUPPORTED_PHRASE);
  hmput(code_to_phrase, HTTP_CONTENT_LENGTH_REQUIRED,
        HTTP_CONTENT_LENGTH_REQUIRED_PHRASE);
  hmput(code_to_phrase, HTTP_SERVER_ERROR, HTTP_SERVER_ERROR_PHRASE);
}