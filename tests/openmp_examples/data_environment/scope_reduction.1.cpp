#pragma omp for
#pragma omp single
#pragma omp scope reduction(+:s,nthrs)
#pragma omp masked
#pragma omp parallel
