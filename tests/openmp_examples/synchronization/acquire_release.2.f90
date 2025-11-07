!$omp    parallel num_threads(2) private(thrd, tmp)
!$omp          atomic write release
!$omp          end atomic
!$omp             atomic read acquire
!$omp             end atomic
!$omp    end parallel
