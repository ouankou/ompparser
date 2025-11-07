#pragma omp parallel
#pragma omp for private(i)
#pragma omp single
#pragma omp for private(i,y,error) reduction(+:toobig)
#pragma omp masked
#pragma omp barrier
#pragma omp masked filter(1)
