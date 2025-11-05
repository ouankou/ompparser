!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     parallel do allocate(x_alloc: x) private(x) shared(result_arr)num_threads(8)
!$omp           simd simdlen(16) aligned(x: 64)
!$omp     end parallel do
