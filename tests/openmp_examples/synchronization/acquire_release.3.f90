!$omp    parallel num_threads(2) private(thrd, tmp)
!$omp          flush
!$omp          atomic write
!$omp          end atomic
!$omp             atomic read
!$omp             end atomic
!$omp          flush
!$omp    end parallel
