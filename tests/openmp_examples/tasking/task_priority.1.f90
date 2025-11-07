!$omp    parallel private(i)
!$omp    single
!$omp       task priority(i)
!$omp       end task
!$omp    end single
!$omp    end parallel
