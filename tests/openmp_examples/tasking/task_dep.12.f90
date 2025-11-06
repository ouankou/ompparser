!$omp    parallel
!$omp    single
!$omp       task shared(x) depend(out: x)
!$omp       end task
!$omp       task shared(x) depend(inout: x) if(.false.)
!$omp       end task
!$omp    end single
!$omp    end parallel
