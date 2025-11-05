!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target map(to: isSharedMemory)
!$omp     end target
!$omp       target firstprivate(fptr)
!$omp       end target
