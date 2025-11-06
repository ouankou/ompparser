#pragma omp target data map(to: v1[0:N], v2[:N]) map(from: p[0:N])
#pragma omp target
#pragma omp parallel for
