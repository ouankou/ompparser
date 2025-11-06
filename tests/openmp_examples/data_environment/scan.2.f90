!$omp    parallel do simd reduction(inscan,+: x)
!$omp       scan exclusive(x)
