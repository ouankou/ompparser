!$omp parallel
!$omp      do
!$omp          task
!$omp                      task
!$omp                      end task
!$omp          end task
!$omp end parallel
