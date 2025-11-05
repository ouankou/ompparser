!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     parallel num_threads(8                     ) shared(x, y, z, num_threads)
!$omp     master taskloop
!$omp     end master taskloop
!$omp     end parallel
