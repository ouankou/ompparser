#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(to: mySelf[0:1])
#pragma omp target enter data map(to: helper_harray[0:size])
#pragma omp target exit data map(from: helper_harray[0:size])
#pragma omp target exit data map(from: mySelf[0:1])
#pragma omp target defaultmap(tofrom:scalar)
#pragma omp target map(from:array_copy[0:1000], sum_copy) defaultmap(tofrom:scalar)
#pragma omp target map (from: _ompvv_isOffloadingOn)
