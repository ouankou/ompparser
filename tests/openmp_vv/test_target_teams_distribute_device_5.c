#pragma omp target enter data map(to: a[dev][0:1024], b[0:1024], num_teams[dev:1]) device(dev)
