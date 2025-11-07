!$omp    parallel do simd reduction(inscan,+: x)
!$omp       scan inclusive(x)
