#pragma omp target exit data map(delete: a[0:1024]) map(from: b[0:1024])
