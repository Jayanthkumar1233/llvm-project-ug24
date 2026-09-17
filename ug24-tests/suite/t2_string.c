#include <stdio.h>
#include <string.h>

int main(void)
{
    char a[32]; char b[32]; char overlap[32] = "0123456789";
    memset(a, 'A', 5); a[5] = '\0';
    memcpy(b, a, 6);
    printf("memset: %s\n", a);
    printf("memcpy: %s\n", b);
    printf("memcmp: %d\n", memcmp(a, b, 6));
    strcpy(a, "hello");
    strncpy(b, "world", sizeof(b)); b[sizeof(b) - 1] = '\0';
    printf("strcpy: %s\n", a);
    printf("strncpy: %s\n", b);
    strcat(a, " uG24");
    printf("strcat: %s\n", a);
    printf("strlen: %u\n", (unsigned)strlen(a));
    printf("strcmp: %d\n", strcmp("abc", "abd"));
    printf("strncmp: %d\n", strncmp("abcdef", "abcxyz", 3));
    printf("strchr: %s\n", strchr(a, '2') ? "found" : "not found");
    memmove(overlap + 2, overlap, 8);
    printf("memmove: %s\n", overlap);
    return 0;
}
