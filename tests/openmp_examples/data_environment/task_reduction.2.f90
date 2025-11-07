!$omp    parallel num_threads(M) reduction(task,+:x)
!$omp      single
!$omp          task in_reduction(+:x)
!$omp          end task
!$omp      end single
!$omp    end parallel
!$omp    parallel do num_threads(M) reduction(task,+:x)
!$omp            task in_reduction(+:x)
!$omp            end task
