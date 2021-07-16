#include <omp.h>
#include <stdio.h>

int main() {
  omp_set_num_threads(1);

  int count = 0;

  // Num threads on parallel overrides set_num_threads
#pragma omp parallel num_threads(4)
  { count = omp_get_thread_num(); }

  printf("%d\n", count);
}