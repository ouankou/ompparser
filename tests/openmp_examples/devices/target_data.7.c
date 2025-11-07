#pragma omp target data map(from: p[0:N])
#pragma omp target if (N>1000000) map(to: v1[:N], v2[:N])
#pragma omp parallel for
