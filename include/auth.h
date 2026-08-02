#include "cc_hashtable.h"
#include "http.h"
#include "server.h"
#include "uuid/uuid.h"

typedef struct Auth_s Auth;


typedef struct
{
    char *username;
    char *role;
    char * sessionID;
} User;

extern thread_local User current_user;
extern Auth auth;
extern CC_HashTable *sessionIDToCSRF;

#define AUTH_COOKIE_REMEMBER_ME_NAME "auth_remember_me"
#define AUTH_COOKIE_USERNAME_NAME "username"
#define AUTH_COOKIE_ROLE_NAME "role"
#define AUTH_COOKIE_SESSIONID_NAME "session_id"
#define AUTH_CSRF_FORM_TOKEN_NAME "csrf"
#define AUTH_CSRF_HEADER_TOKEN_NAME "X-CSRF-TOKEN"
#define AUTH_CSRF_TOKEN_SIZE 37
#define AUTH_SESSION_TOKEN_SIZE 37

void initAuth(Auth* auth);

int loginUser(Auth *auth, char *username, char *password, char **token, char** role);
int signUpUser(Auth *auth, char *username, char *password,char *role, char **token);
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
    char *adminHTMLLocation;
    char *adminURLPrefix;
} AuthHandler;

void initAuthHandler(AuthHandler *handler,
                     char *loginURI,
                     char *signUpURI,
                     char *adminURLPrefix,
                     char *adminHTMLLocation);

void addAuthenticationHandler(Server *server, AuthHandler *authHandler);
int authFilter(HTTPRequest *request, HTTPResponse *response, int connfd, void* args);
void addRoleFilter(Route* route,  char** roles, int n);
int sessionIDFilter(HTTPRequest *request, HTTPResponse *response, int connfd, void* args);
int csrfFilter(HTTPRequest *request, HTTPResponse *response, int connfd, void * args);
char * generateSessionID();
Route getCSRFRoute();
