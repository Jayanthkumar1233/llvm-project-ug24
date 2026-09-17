#include <ug24.h>

int add(int a, int b) { return a + b; }

int main(void) {
  int x = 25;
  int y = 17;
  int sum = add(x, y);

  ug24_puts("Addition on uG24\n");

  ug24_puts("  a   = ");
  ug24_put_s16(x);
  ug24_putchar('\n');
  ug24_puts("  b   = ");
  ug24_put_s16(y);
  ug24_putchar('\n');
  ug24_puts("  a+b = ");
  ug24_put_s16(sum);
  ug24_putchar('\n');

  return sum;
}