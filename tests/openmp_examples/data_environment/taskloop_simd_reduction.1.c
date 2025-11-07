#pragma omp parallel masked
#pragma omp taskloop reduction(+:asum)
#pragma omp parallel reduction(task, +:asum)
#pragma omp masked
#pragma omp task in_reduction(+:asum)
#pragma omp masked taskloop in_reduction(+:asum)
#pragma omp parallel masked
#pragma omp taskloop simd reduction(+:asum)
#pragma omp parallel reduction(task, +:asum)
#pragma omp masked
#pragma omp task in_reduction(+:asum)
#pragma omp masked taskloop simd in_reduction(+:asum)
