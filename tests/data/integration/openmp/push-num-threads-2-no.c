#include <omp.h>
#include <stdio.h>

int main() {
  omp_set_num_threads(1);

  int shared = 0;

  // Num threads on parallel overrides set_num_threads
#pragma omp parallel num_threads(4)
  { int local = omp_get_thread_num(); }

  // No race here because num threads is set to 1
#pragma omp parallel
  { shared = omp_get_thread_num(); }

  printf("%d\n", shared);
}