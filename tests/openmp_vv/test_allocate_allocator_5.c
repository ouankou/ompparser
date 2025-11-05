#pragma omp parallel for simd simdlen(16) aligned(x, y: 64)
