#pragma omp begin declare target
#pragma omp end declare target
#pragma omp task shared(Z)
#pragma omp target map(Z[C:1000000])
#pragma omp parallel for
#pragma omp taskwait
