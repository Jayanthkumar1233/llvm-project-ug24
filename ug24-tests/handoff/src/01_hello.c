/* 01_hello - the UART and the halt, and nothing else.
 *
 * No formatting and no arithmetic: clang turns printf of a plain string into
 * puts, so this program is a handful of stores to the transmit register
 * followed by the end of main.  If a simulator prints these three lines and
 * stops, its instruction decode, its call and return, and its console are all
 * working.  If it prints nothing, the console address is the thing to check
 * first.  If it prints them and then hangs, WFI is the thing to check.
 */
#include <stdio.h>

int main(void)
{
    printf("uG24 sample 1: console\n");
    printf("  the second line\n");
    printf("  the third line\n");
    return 0;
}
