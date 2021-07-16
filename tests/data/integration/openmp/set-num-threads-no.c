#include <omp.h>
#include <stdio.h>

int main() {
  omp_set_num_threads(1);

  int count = 0;

#pragma omp parallel
  { count++; }

  printf("%d\n", count);
}