!$omp   parallel private(i,v) shared(R)
!$omp   single
!$omp       task depend(inout: R)
!$omp       end task
!$omp     task depend(in: R)
!$omp     end task
!$omp     taskwait
!$omp       task depend(inoutset: R)
!$omp         atomic
!$omp       end task
!$omp     task depend(in: R)
!$omp     end task
!$omp   end single
!$omp   end parallel
