!$omp     task depend(inout: x) shared(x)
!$omp     end task
!$omp     task shared(y)
!$omp     end task
!$omp     taskwait depend(in: x)
!$omp     taskwait
!$omp     parallel
!$omp     single
!$omp     end single
!$omp     end parallel
