!$omp     parallel do simd shared(x) num_threads(8)
!$omp        atomic update
!$omp     end parallel do simd
