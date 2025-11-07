#pragma omp for ordered(1)
#pragma omp ordered doacross(sink: i-1)
#pragma omp ordered doacross(source: omp_cur_iteration)
