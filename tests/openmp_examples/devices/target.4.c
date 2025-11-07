#pragma omp target map(to: v1[0:N], v2[:N]) map(from: p[0:N])
#pragma omp parallel for
