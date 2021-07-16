#include <omp.h>
#include <stdio.h>

int main() {
  omp_set_num_threads(1);

  int count = 0;

#pragma omp parallel
  { count++; }

  omp_set_num_threads(2);

#pragma omp parallel
  { count = omp_get_thread_num(); }

  printf("%d\n", count);
}