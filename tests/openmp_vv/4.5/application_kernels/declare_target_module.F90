!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     declare target
!$omp   target if(x > THRESHOLD) map(tofrom: x)
!$omp   end target
