#pragma omp target data map(from: a3d[:1000 - 2][:2][:2]) map(from: a3d2[:1000][:2][:2])
