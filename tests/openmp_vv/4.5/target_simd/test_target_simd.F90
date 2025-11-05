!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp             target simd map(to: b(1:1024      ), c(1:1024      ))map(tofrom: a(1:1024))
!$omp             end target simd
