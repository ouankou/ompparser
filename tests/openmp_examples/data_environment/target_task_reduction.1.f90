!$omp       declare target enter(device_compute)
!$omp    parallel masked
!$omp       taskgroup task_reduction(+:sum)
!$omp          target in_reduction(+:sum) nowait
!$omp          end target
!$omp          task in_reduction(+:sum)
!$omp          end task
!$omp       end taskgroup
!$omp    end parallel masked
!$omp    declare target enter(device_compute)
