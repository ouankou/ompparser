#pragma omp target update from(s.data[:1024/2:2])
