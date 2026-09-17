#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int main(void)
{
    uint8_t *a; uint8_t *b;

    a = malloc(32);
    if (!a) { printf("malloc failed\n"); return 1; }
    memset(a, 0xAA, 32);
    printf("a=%02X %02X\n", a[0], a[31]);

    b = calloc(16, 1);
    if (!b) { printf("calloc failed\n"); free(a); return 1; }
    printf("b=%02X %02X\n", b[0], b[15]);

    a = realloc(a, 64);
    if (!a) { printf("realloc failed\n"); free(b); return 1; }
    printf("realloc=%02X\n", a[0]);

    free(a); free(b);
    return 0;
}
