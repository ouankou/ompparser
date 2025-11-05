!$omp     target map(iterator(it = 1:1024), tofrom: test_lst(it)%ptr) map(test_lst)
