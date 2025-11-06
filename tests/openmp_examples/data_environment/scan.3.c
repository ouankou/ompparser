#pragma omp parallel for simd reduction(inscan,+: x) private(tmp)
#pragma omp scan init_complete
#pragma omp scan exclusive(x)
