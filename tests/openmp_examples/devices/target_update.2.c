#pragma omp target data map(to: v1[:N], v2[:N]) map(from: p[0:N])
#pragma omp target
#pragma omp parallel for
#pragma omp target update if (changed) to(v1[:N])
#pragma omp target update if (changed) to(v2[:N])
#pragma omp target
#pragma omp parallel for
