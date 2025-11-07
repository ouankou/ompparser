!$omp   parallel reduction(+: x) num_threads(strict: 4)
!$omp     do reduction(+: x)
!$omp     end do
!$omp   end parallel
