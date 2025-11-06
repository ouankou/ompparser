!$omp    parallel num_threads(2) private(thrd, tmp)
!$omp          critical
!$omp          end critical
!$omp             critical
!$omp             end critical
!$omp    end parallel
