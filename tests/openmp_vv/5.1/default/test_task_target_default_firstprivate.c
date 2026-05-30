#pragma omp target map(tofrom: test_num, test_arr)
#pragma omp task default(firstprivate)
#pragma omp target map (from: _ompvv_isOffloadingOn)
