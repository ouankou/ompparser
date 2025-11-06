#pragma omp target data map(to: v1[:N], v2[:N]) map(from: p[0:N])
#pragma omp target
#pragma omp parallel for
#pragma omp target update to(v1[:N], v2[:N])
#pragma omp target
#pragma omp parallel for
