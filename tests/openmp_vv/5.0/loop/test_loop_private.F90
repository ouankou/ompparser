!$omp   parallel num_threads(8                     )
!$omp   loop private(privatized)
!$omp   end loop
!$omp   end parallel
