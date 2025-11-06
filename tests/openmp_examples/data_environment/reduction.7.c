#pragma omp parallel for reduction(+:a[0:100]) private(j)
