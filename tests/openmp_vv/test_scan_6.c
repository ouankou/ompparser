#pragma omp parallel for simd reduction(inscan, +: x) num_threads(8)
