!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     declare target to(parallel_for_fun)
!$omp       declare target to(parallel_for_fun)
!$omp       target map(tofrom: x, y, z, num_threads)
!$omp       end target
