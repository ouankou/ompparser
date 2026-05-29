!$omp     target parallel num_threads(8                       ) map(tofrom: a, num_threads, wait_errors) map(to: b, c)
!$omp     loop order(concurrent)
!$omp        atomic update
!$omp     end target parallel
