#pragma omp target enter data map(to: s.len, s.data[0:s.len])
