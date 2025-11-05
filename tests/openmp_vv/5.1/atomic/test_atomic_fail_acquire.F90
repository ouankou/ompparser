!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     parallel num_threads(2) private(thrd)
!$omp       atomic write seq_cst
!$omp       end atomic
!$omp         atomic compare seq_cst fail(acquire)
!$omp         end atomic
!$omp     end parallel
