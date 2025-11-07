#pragma omp target data map(Q[0:rows][0:cols])
#pragma omp target map(tofrom: tmp)
#pragma omp parallel for reduction(+:tmp)
#pragma omp target
#pragma omp parallel for
