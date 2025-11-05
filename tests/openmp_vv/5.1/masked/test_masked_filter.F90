!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     parallel num_threads(threads) private(tot)
!$omp       atomic read
!$omp       masked filter(3)
!$omp       end masked
!$omp     end parallel
