#pragma omp target map(tofrom: a, sum) depend(out: a) nowait
