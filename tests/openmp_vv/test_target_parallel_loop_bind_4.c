#pragma omp target parallel loop bind(parallel) map(tofrom: arr) num_threads(8) private(j,k)
