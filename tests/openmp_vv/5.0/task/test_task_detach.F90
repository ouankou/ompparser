!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp       parallel
!$omp       single
!$omp       task depend(out: y) detach(flag_event)
!$omp       end task
!$omp       task
!$omp       flush
!$omp       end task
!$omp       task depend(inout: y)
!$omp       flush
!$omp       end task
!$omp       end single
!$omp       end parallel
