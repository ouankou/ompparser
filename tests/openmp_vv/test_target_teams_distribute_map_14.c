#pragma omp target teams distribute map(tofrom: b[0:1024], a[0:1024])
