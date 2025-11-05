#pragma omp target parallel for collapse(2) shared(A, B) device(gpu)
