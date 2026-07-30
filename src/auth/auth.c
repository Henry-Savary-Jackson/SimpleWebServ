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

thread_local User current_user = {NULL, NULL};
CC_HashTable *sessionIDToCSRF = NULL;

unsigned char publicKey[crypto_sign_PUBLICKEYBYTES];
unsigned char secretKey[crypto_sign_SECRETKEYBYTES];

struct Auth_s
{
    char *username;
    char *passwordHash;
    char *role;
};

Auth auth = {
    "admin",
    NULL,
    "admin",
};

#define MAX_USERNAME 128

int initKeyPair(){
    return crypto_sign_keypair(publicKey, secretKey);
}

int generateLoginToken(char* username, char **token){
    int message_len = strlen(username);
    unsigned char output[MAX_USERNAME+ crypto_sign_BYTES];
    unsigned long long signed_message_len;
    crypto_sign_detached(output,&signed_message_len, (unsigned char*)username, message_len, secretKey );
    *token =custom_alloc(signed_message_len+1);
    memcpy(*token, output ,signed_message_len);
    // null terminate it
    *token[signed_message_len] = 0;
    return 0;
}

// TODO: make sure the memory for storing the sensitive request ( )
int loginUser(Auth* auth, char *username, char *password, char** token)
{
    if (crypto_pwhash_str_verify(auth->passwordHash, password, strlen(password)) != 0){
        return -1;
    }

    current_user.username = auth->username;
    current_user.role = auth->role;

    sodium_memzero(password, strlen(password));

    generateLoginToken(username, token);
    return 0;
}


int signUpUser(Auth* auth, char *username, char *password, char **token)
{
    char hashed_password[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(hashed_password,
                          password,
                          strlen(password),
                          crypto_pwhash_OPSLIMIT_SENSITIVE,
                          crypto_pwhash_MEMLIMIT_SENSITIVE) != 0)
    {
        return -1;
    }
    auth->username =  custom_strdup(username);
    auth->passwordHash = custom_strdup(hashed_password);
    // generateLoginToken(username, token);
    return 0;
}

int checkUserToken(Auth* auth, char *token)
{
    unsigned long long tokenLength= strlen(token);

    unsigned char username[MAX_USERNAME];
    unsigned long long unsigned_message_len;
    if (crypto_sign_open(username, &unsigned_message_len,
                        (u_char*)token, tokenLength, publicKey) != 0) {
        return -1;
    }
    current_user.username = custom_strdup((char*)username);
    current_user.role = auth->role;
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
