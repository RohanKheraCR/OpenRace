#include <omp.h>
#include <stdio.h>

int main() {
  int count = 0;

  // Num threads is set to one so there is no race
#pragma omp parallel num_threads(1)
  { count++; }

  printf("%d\n", count);
}