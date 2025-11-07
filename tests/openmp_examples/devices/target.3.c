#pragma omp target map(to: v1, v2) map(from: p)
#pragma omp parallel for
