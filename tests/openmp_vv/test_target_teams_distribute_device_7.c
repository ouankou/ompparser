#pragma omp target exit data map(from: a[dev][0:1024], num_teams[dev:1]) map(delete: b[0:1024]) device(dev)
