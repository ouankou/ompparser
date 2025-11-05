!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target defaultmap(to)
!$omp     end target
!$omp     target defaultmap(from)
!$omp     end target
!$omp     target defaultmap(tofrom)
!$omp     end target
