!$omp     task depend(inout: x) shared(x)
!$omp     end task
!$omp     task depend(in: x) depend(inout: y) shared(x, y)
!$omp     end task
!$omp     taskwait depend(in: x,y)
!$omp     parallel
!$omp     single
!$omp     end single
!$omp     end parallel
