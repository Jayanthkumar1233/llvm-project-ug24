//===-- string.h - String and memory helpers for the uG24 -----------------===//
#ifndef _UG24_STRING_H
#define _UG24_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memcpy(void *__dst, const void *__src, unsigned __size);
void *memmove(void *__dst, const void *__src, unsigned __size);
void *memset(void *__dst, int __value, unsigned __size);
int   memcmp(const void *__a, const void *__b, unsigned __size);

unsigned strlen(const char *__s);
int   strcmp(const char *__a, const char *__b);
int   strncmp(const char *__a, const char *__b, unsigned __n);
char *strcpy(char *__dst, const char *__src);
char *strncpy(char *__dst, const char *__src, unsigned __n);
char *strcat(char *__dst, const char *__src);
char *strchr(const char *__s, int __c);

#ifdef __cplusplus
}
#endif

#endif // _UG24_STRING_H
