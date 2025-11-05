#pragma omp target exit data map(delete: s.len, s.data[0:s.len])
