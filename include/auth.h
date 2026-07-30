#include "cc_hashtable.h"
#include "http.h"
#include "server.h"
#include "uuid/uuid.h"

typedef struct Auth_s Auth;


typedef struct
{
    char *username;
    char *role
} User;

extern thread_local User current_user;
extern Auth auth;
extern CC_HashTable *sessionIDToCSRF;

#define AUTH_COOKIE_REMEMBER_ME_NAME "auth_remember_me"
#define AUTH_COOKIE_SESSIONID_NAME "session_id"
#define AUTH_CSRF_FORM_TOKEN_NAME "csrf"
#define AUTH_CSRF_HEADER_TOKEN_NAME "X-CSRF-TOKEN"
#define AUTH_CSRF_TOKEN_SIZE 37
#define AUTH_SESSION_TOKEN_SIZE 37


int loginUser(Auth *auth, char *username, char *password, char **token);
int signUpUser(Auth *auth, char *username, char *password, char **token);
int checkUserToken(Auth *auth, char *token);
int initKeyPair();
int saveKeyPair();
int loadKeyPair();

char *getCSRFForSession(char *sessionId);
char *addCSRFToSession(char *sessionId);

typedef struct
{
    char *loginURI;
    char *signUpURI;
    char *loginFormFileLocation;
    char *signUpFormFileLocation;
} AuthHandler;

void initAuthHandler(AuthHandler *handler,
                     char *loginURI,
                     char *signUpURI,
                     char *loginFormFileLocation,
                     char *signUpFormFileLocation);

void addAuthenticationHandler(Server* server, AuthHandler *authHandler);
int authFilter(HTTPRequest *request, HTTPResponse *response, int connfd);
int sessionIDFilter(HTTPRequest *request, HTTPResponse *response, int connfd);
int csrfFilter(HTTPRequest *request, HTTPResponse *response, int connfd);
Route getCSRFRoute();
