!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     parallel do simd shared(x) num_threads(8)
!$omp        atomic update
!$omp     end parallel do simd
