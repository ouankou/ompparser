#pragma omp target data map(from: p[0:N])
#pragma omp target map(to: v1[:N], v2[:N])
#pragma omp parallel for
#pragma omp target map(to: v1[:N], v2[:N])
#pragma omp parallel for
