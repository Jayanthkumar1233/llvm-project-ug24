#include <stdio.h>
#include <string.h>
int main(void) {
    printf("[%d] [%d] [%d]\n", 0, -1, 32767);
    printf("[%u] [%u]\n", 0u, 65535u);
    printf("[%5d] [%-5d|] [%05d]\n", 42, 42, 42);
    printf("[%+d] [% d] [%+d]\n", 42, 42, -42);
    printf("[%x] [%X] [%#x] [%04X]\n", 48879u, 48879u, 255u, 255u);
    printf("[%o] [%#o]\n", 8u, 8u);
    printf("[%c] [%3c] [%-3c|]\n", 'A', 'B', 'C');
    printf("[%s] [%8s] [%-8s|] [%.3s]\n", "hi", "hi", "hi", "abcdef");
    printf("[%s]\n", (char *)0);
    printf("[%ld] [%lu] [%lX]\n", -100000L, 100000UL, 0xDEADBEEFUL);
    printf("[%%] [%*d]\n", 6, 7);
    char buf[32];
    int n = snprintf(buf, sizeof buf, "%d-%s-%x", 12, "ab", 255u);
    printf("snprintf n=%d buf=[%s] len=%u\n", n, buf, strlen(buf));
    printf("strcmp=%d memcmp=%d\n", strcmp("abc","abd"), memcmp("ab","ab",2));
    return 0;
}
