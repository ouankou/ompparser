#pragma omp target teams distribute firstprivate(privatized_array, privatized) map(alloc: a[0:1024], b[0:1024], c[0:1024], d[0:1024]) num_teams(8)
