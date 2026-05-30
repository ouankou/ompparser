!$omp     parallel num_threads(8                     )
!$omp     loop order(concurrent)
!$omp        atomic update
!$omp     end parallel
