!$omp   parallel shared(a) num_threads(2)
!$omp   end parallel
!$omp   parallel shared(a) num_threads(2)
!$omp   end parallel
!$omp     atomic
!$omp     atomic
