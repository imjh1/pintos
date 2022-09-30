#include <stdio.h>
#include <string.h>
#include <syscall.h>

int
main (int argc, char *argv[])
{
  int num[4];

  for(int i=1; i<argc; i++){
    int x = 0;
    int size = strlen(argv[i]);
    int power = 1;

    for(int j=1; j<size; j++)
      power *= 10;

    for(int j=0; j<size; j++){
      x += (argv[i][j] - '0') * power;
      power /= 10;
    }

    num[i-1] = x;
  }

  printf("%d %d\n", fibonacci(num[0]), max_of_four_int(num[0], num[1], num[2], num[3]));
}
