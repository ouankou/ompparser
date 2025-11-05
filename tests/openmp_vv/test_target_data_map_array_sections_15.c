#pragma omp target map(alloc: a3d[:1000 - 2][:2][:2], a3d2[:1000][:2][:2])
