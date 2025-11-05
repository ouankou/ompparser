!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp   target map(tofrom: sum) map(to: a)
!$omp   end target
!$omp   declare target
