!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp   requires atomic_default_mem_order(relaxed)
!$omp     parallel private(thrd, tmp)
!$omp           flush
!$omp           atomic write
!$omp           end atomic
!$omp             atomic read
!$omp             end atomic
!$omp     end parallel
