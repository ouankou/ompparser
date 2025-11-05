!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target map(tofrom: x, y, z, num_threads)
!$omp     parallel master taskloop num_threads(8                       )shared(x, y, z, num_threads)
!$omp     end parallel master taskloop
!$omp     end target
