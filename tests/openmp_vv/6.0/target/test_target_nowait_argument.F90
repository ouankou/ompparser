!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target map(tofrom: x) nowait(is_deferred)
!$omp     end target
!$omp       taskwait
!$omp   declare target
