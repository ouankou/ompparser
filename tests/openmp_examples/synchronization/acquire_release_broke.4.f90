!$omp    parallel num_threads(2) private(thrd) private(tmp)
!$omp          critical
!$omp          end critical
!$omp          atomic write
!$omp          end atomic
!$omp            atomic read acquire
!$omp            end atomic
!$omp          critical
!$omp          end critical
!$omp    end parallel
