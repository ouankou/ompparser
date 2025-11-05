!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     parallel num_threads(8                     )
!$omp     loop order(concurrent)
!$omp        atomic update
!$omp     end parallel
