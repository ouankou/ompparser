#pragma omp parallel for simd shared(x) num_threads(8)
#pragma omp atomic update
