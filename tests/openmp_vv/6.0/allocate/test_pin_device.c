#pragma omp target map(tofrom: arr[0:1024])
#pragma omp target map (from: _ompvv_isOffloadingOn)
