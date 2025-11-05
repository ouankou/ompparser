!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp   simd simdlen(64) if(k .eq. 1024)
!$omp   simd simdlen(64) if(k .ne. 1024)
