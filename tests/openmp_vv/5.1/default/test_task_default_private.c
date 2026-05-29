#pragma omp task shared(sum) default(private)
#pragma omp target map(tofrom: sum, test_num)
#pragma omp target map (from: _ompvv_isOffloadingOn)
