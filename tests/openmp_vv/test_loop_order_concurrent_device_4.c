#pragma omp target parallel num_threads(8) map(tofrom: x[0:1024], num_threads, total_wait_errors) map(to: y[0:1024], z[0:1024])
