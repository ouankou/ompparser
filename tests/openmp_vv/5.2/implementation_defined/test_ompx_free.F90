!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp                 parallel shared(ARR_ERR) private(i)  num_threads(2)
!$omp                 end parallel
