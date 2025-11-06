#pragma omp begin declare target
#pragma omp end declare target
#pragma omp target update to(Q)
#pragma omp target map(tofrom: tmp)
#pragma omp parallel for reduction(+:tmp)
