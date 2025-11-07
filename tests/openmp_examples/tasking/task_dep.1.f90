!$omp    parallel
!$omp    single
!$omp       task shared(x) depend(out: x)
!$omp       end task
!$omp       task shared(x) depend(in: x)
!$omp       end task
!$omp    end single
!$omp    end parallel
