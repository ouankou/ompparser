!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     parallel private(p_val) shared(actualThreadCnt)
!$omp     target map(tofrom:compute_array(:, p_val)) firstprivate(p_val)
!$omp     end target
!$omp     end parallel
