#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(iterator(it = 0:1024), tofrom: test_lst[it][:1]) map(to: test_lst)
#pragma omp target map (from: _ompvv_isOffloadingOn)
