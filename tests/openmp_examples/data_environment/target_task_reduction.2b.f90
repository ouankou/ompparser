!$omp       declare target enter(device_compute)
!$omp    parallel masked reduction(task,+:sum)
!$omp          target in_reduction(+:sum) nowait
!$omp          end target
!$omp    end parallel masked
!$omp    declare target enter(device_compute)
