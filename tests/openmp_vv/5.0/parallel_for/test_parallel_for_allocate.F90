!$omp     parallel do allocate(x_alloc: x) private(x) shared(result_arr)num_threads(8)
!$omp           simd simdlen(16) aligned(x: 64)
!$omp     end parallel do
