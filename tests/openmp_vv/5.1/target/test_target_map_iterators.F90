!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target map(iterator(it = 1:1024), tofrom: test_lst(it)%ptr) map(test_lst)
!$omp     end target
