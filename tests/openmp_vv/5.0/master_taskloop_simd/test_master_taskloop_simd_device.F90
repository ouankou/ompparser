!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target parallel num_threads(8                       ) shared(x, y, z, num_threads) map(tofrom: x, num_threads) map(to: y, z)
!$omp     master taskloop simd
!$omp     end master taskloop simd
!$omp     end target parallel
