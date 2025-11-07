#pragma omp parallel
#pragma omp masked
#pragma omp taskloop
#pragma omp parallel masked taskloop
#pragma omp parallel masked taskloop simd
