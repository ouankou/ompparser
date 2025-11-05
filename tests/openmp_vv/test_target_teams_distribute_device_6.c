#pragma omp target teams distribute map(alloc: a[dev][0:1024], b[0:1024], num_teams[dev:1]) device(dev)
