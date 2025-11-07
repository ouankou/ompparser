!$omp   do reduction(original(private),+: sum_v)
!$omp   parallel private(s_v)
!$omp   end parallel
