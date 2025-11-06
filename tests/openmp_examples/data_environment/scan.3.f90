!$omp    parallel do simd reduction(inscan,+: x) private(tmp)
!$omp       scan init_complete
!$omp       scan exclusive(x)
