!$omp   do reduction(+: sum_v)
!$omp   parallel private(s_v)
!$omp   end parallel
