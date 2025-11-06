#pragma omp target data map(to: v1[0:N], v2[:N]) map(from: p0[0:N])
#pragma omp target map(to: v3[0:N], v4[:N]) map(from: p1[0:N])
#pragma omp parallel for
