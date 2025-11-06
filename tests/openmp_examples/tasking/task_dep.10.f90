!$omp    parallel
!$omp    single
!$omp       task depend(out: a)
!$omp       end task
!$omp       task depend(out: b)
!$omp       end task
!$omp       task depend(in: a) depend(mutexinoutset: c)
!$omp       end task
!$omp       task depend(in: b) depend(mutexinoutset: c)
!$omp       end task
!$omp    end single
!$omp    end parallel
