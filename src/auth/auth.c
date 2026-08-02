#include "cc_common.h"
#include "cc_hashtable.h"
#include "http.h"
#include "server.h"
#include <argon2.h>
#include <auth.h>
#include <sodium.h>
#include <sodium/crypto_pwhash.h>
#include <sodium/crypto_pwhash_argon2i.h>
#include <sodium/crypto_sign.h>
#include <sodium/crypto_sign_ed25519.h>
#include <sodium/utils.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define KEY_PATH "keys.bin"

thread_local User current_user = {NULL, NULL, NULL};
CC_HashTable *sessionIDToCSRF = NULL;

unsigned char publicKey[crypto_sign_PUBLICKEYBYTES];
unsigned char secretKey[crypto_sign_SECRETKEYBYTES];

struct Auth_s
{
    CC_HashTable* users;
};


typedef struct {
    char * username;
    char * passwordHash;
    char * role;
} UserRecord;

Auth auth ;

#define MAX_USERNAME 128

void initAuth(Auth* auth){
    CC_HashTableConf conf;
    configureHTTPDictGlobal(&conf);
    cc_hashtable_new_conf(&conf,&auth->users);
}

UserRecord* getUser(Auth* auth, char* username){
    UserRecord* out = NULL;
    enum cc_stat stat = cc_hashtable_get(auth->users,username, (void**)&out );
    if (stat != CC_OK){
        return NULL;
    }
    return out;
}

UserRecord* addUser(Auth* auth, char * username, char * passwordHash,char* role){
    UserRecord* user= glbl_custom_alloc(sizeof(User));

    user->username = glbl_custom_strdup( username);
    user->passwordHash =glbl_custom_strdup( passwordHash);
    user->role = glbl_custom_strdup(role);
    enum cc_stat stat = cc_hashtable_add(auth->users,user->username, user );
    if (stat != CC_OK){
        return NULL;
    }
    return user;
}

int initKeyPair(){
    return crypto_sign_keypair(publicKey, secretKey);
}

char * generateSessionID(){
    uuid_t binSessionID;
    uuid_generate_random(binSessionID);
    char *newSessionID = custom_alloc(AUTH_SESSION_TOKEN_SIZE);
    uuid_unparse_lower(binSessionID, newSessionID);
    return newSessionID;
}

int generateLoginToken(char* username, char **token){
    int message_len = strlen(username);
    unsigned char output[MAX_USERNAME+ crypto_sign_BYTES];
    unsigned long long signed_message_len;
    crypto_sign(output,&signed_message_len, (unsigned char*)username, message_len, secretKey );
    output[signed_message_len] = 0 ;

    size_t lengthToken =sodium_base64_encoded_len( signed_message_len, sodium_base64_VARIANT_URLSAFE);
    *token = custom_alloc(lengthToken);
    *token = sodium_bin2base64(*token, lengthToken, output, signed_message_len, sodium_base64_VARIANT_URLSAFE);
    return 0;
}

// TODO: make sure the memory for storing the sensitive request ( )
int loginUser(Auth* auth, char *username, char *password, char** token, char** role)
{
    UserRecord* record = getUser(auth, username);
    if (!record){
        return -2;
    }
    if (crypto_pwhash_str_verify(record->passwordHash, password, strlen(password)) != 0){
        return -1;
    }

    current_user.username = record->username ;
    current_user.role = record->role;

    *role = record->role;

    sodium_memzero(password, strlen(password));

    generateLoginToken(username, token);
    return 0;
}


int signUpUser(Auth* auth, char *username, char *password, char* role, char **token)
{
    char hashed_password[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(hashed_password,
                          password,
                          strlen(password),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
    {
        return -1;
    }

    addUser(auth, username, hashed_password, role);
    return 0;
}

int checkUserToken(Auth* auth, char *token)
{
    unsigned long long tokenLength= strlen(token);
    u_char tokenBin[strlen(token)+1];
    size_t binLength = 0;
    int ret = sodium_base642bin((u_char*)tokenBin, sizeof(tokenBin), token, tokenLength, NULL, &binLength, NULL, sodium_base64_VARIANT_URLSAFE );
    if (ret){
        return ret;
    }

    unsigned char username[MAX_USERNAME];
    unsigned long long unsigned_message_len;
    if (crypto_sign_open(username, &unsigned_message_len,
                        tokenBin, binLength, publicKey) != 0) {
        return -1;
    }
    username[unsigned_message_len] = 0; // nulterm
    UserRecord* record =  getUser(auth, (char*)username);
    if (!record){
        return -2;
    }
    current_user.username = record->username;
    current_user.role = record->role;
    return 0;
}

int saveKeyPair(){

    FILE* keyFile = fopen(KEY_PATH,"w+");
    if (!keyFile){
        return -1;
    }

    int result =writeToFile(keyFile, (char*)secretKey, crypto_sign_SECRETKEYBYTES);
    if (result < 0){
        return -1;
    }

    fclose(keyFile);
    return 0;
}
int loadKeyPair(){

    if (access(KEY_PATH, F_OK)){
        crypto_sign_keypair(publicKey, secretKey);
        saveKeyPair();
        return 0;
    }

    FILE* keyFile = fopen(KEY_PATH,"r");
    if (!keyFile){
        return -1;
    }

    int result = readFromFile(keyFile, (char*)secretKey, crypto_sign_SECRETKEYBYTES);
    if (result < 0){
        return result;
    }

    result = crypto_sign_ed25519_sk_to_pk(publicKey,secretKey);
    if (result ){
        return result;
    }

    fclose(keyFile);
    return 0;
}
