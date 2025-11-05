!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     parallel master num_threads(8                     ) shared(x,y, z, num_threads)
!$omp     taskloop
!$omp     end taskloop
!$omp     end parallel master
