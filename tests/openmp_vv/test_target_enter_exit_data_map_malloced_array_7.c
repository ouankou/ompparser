#pragma omp target data map(tofrom: x[:10]) map(from: y[:10])
