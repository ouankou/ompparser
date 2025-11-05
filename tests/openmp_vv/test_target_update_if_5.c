#pragma omp target data map(to: a[:100], b[:100]) map(tofrom: c)
