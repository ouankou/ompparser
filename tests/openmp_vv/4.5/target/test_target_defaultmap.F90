!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp             declare target
!$omp             declare target
!$omp             declare target
!$omp             target defaultmap(tofrom: scalar)
!$omp             end target
!$omp             target map(tofrom: firstprivateCheck)
!$omp             end target
