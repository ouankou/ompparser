#pragma omp parallel reduction(+: x) num_threads(strict: 4)
#pragma omp for reduction(+: x)
