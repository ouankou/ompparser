#pragma omp parallel for simd reduction(inscan, +: x) num_threads(8)
#pragma omp scan inclusive(x)
#pragma omp parallel for simd reduction(inscan, +: x) num_threads(8)
#pragma omp scan exclusive(x)
