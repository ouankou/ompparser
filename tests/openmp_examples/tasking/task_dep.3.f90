!$omp    parallel
!$omp    single
!$omp       task shared(x) depend(out: x)
!$omp       end task
!$omp       task shared(x) depend(out: x)
!$omp       end task
!$omp       taskwait
!$omp    end single
!$omp    end parallel
