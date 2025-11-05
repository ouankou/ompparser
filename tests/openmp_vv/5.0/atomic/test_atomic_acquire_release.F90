!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     parallel private(thrd, tmp)
!$omp           atomic write release
!$omp           end atomic
!$omp             atomic read acquire
!$omp             end atomic
!$omp     end parallel
