!$omp       declare target enter(device_compute)
!$omp    parallel sections reduction(task,+:sum)
!$omp       section
!$omp          target in_reduction(+:sum) nowait
!$omp          end target
!$omp       section
!$omp    end parallel sections
!$omp    declare target enter(device_compute)
