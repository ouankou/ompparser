#pragma omp parallel for simd reduction(inscan,+: x)
#pragma omp scan inclusive(x)
