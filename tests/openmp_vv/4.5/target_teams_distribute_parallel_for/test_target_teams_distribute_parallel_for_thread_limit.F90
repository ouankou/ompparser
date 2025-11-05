!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp             target teams distribute parallel do map(tofrom:num_threads) num_threads(tested_num_threads(nt)) thread_limit(tested_thread_limit(tl))
