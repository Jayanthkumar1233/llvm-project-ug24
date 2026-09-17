// A small bare-metal uG24 program: fill a table with Fibonacci numbers.
unsigned char buffer[8];

static unsigned char fib(unsigned char n) {
    unsigned char a = 0, b = 1, t;
    while (n--) { t = (unsigned char)(a + b); a = b; b = t; }
    return a;
}

int main(void) {
    for (unsigned char i = 0; i < 8; i++)
        buffer[i] = fib(i);
    return buffer[7];
}
