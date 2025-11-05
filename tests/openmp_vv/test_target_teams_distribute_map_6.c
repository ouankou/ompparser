#pragma omp target teams distribute map(to: a[0:1024], b[0:1024])
