!$omp    masked
!$omp    end masked
!$omp    parallel
!$omp       parallel
!$omp          parallel
!$omp          end parallel
!$omp       end parallel
!$omp    end parallel
!$omp    parallel num_threads(8)
!$omp       parallel
!$omp          parallel
!$omp          end parallel
!$omp       end parallel
!$omp    end parallel
!$omp    parallel num_threads(8,2)
!$omp       parallel
!$omp          parallel
!$omp          end parallel
!$omp       end parallel
!$omp    end parallel
