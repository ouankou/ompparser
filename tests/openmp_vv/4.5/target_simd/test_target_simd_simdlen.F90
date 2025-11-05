!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp             target simd safelen(1) map(tofrom: A(1:1024      ))
!$omp             end target simd
!$omp             target simd safelen(5) map(tofrom: A(1:1024      ))
!$omp             end target simd
!$omp             target simd safelen(8) map(tofrom: A(1:1024      ))
!$omp             end target simd
!$omp             target simd safelen(13) map(tofrom: A(1:1024      ))
!$omp             end target simd
!$omp             target simd safelen(16) map(tofrom: A(1:1024      ))
!$omp             end target simd
!$omp             target simd safelen(100) map(tofrom: A(1:1024      ))
!$omp             end target simd
!$omp             target simd safelen(128) map(tofrom: A(1:1024      ))
!$omp             end target simd
