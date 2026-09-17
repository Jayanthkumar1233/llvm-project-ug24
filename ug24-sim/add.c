#include <stdio.h>
#include <stdlib.h>

int main() {
  printf("Enter the String : ");
  char s[10] = "12345";
  //   gets(s);
  int i = atoi(s);
  int n = 45;
  int *p = &n;
  int arr[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  for (int a = 0; a < 10; a++) {
    printf("%d ", *(arr + a));
  }
}