#pragma once

typedef struct{char * buf; int length;} String;

void initString(String* s);
void freeString(String* s);
void copyString(String* dst, char * src);