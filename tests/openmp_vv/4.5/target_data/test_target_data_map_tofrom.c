#pragma omp target data map(tofrom: h_array_h[0:1000]) map(tofrom : h_array_s[0:1000])
#pragma omp target
#pragma omp target map (from: _ompvv_isOffloadingOn)
