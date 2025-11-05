#pragma omp target data map(to: a[:1024], b[:1024]) map(from: c)
