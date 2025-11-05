!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp   target simd nontemporal(a, b, c)
!$omp   end target simd
