#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(alloc:h_array_h[0:1000]) map(alloc:h_array_s[0:1000])
#pragma omp target map(ptr_h_array_h[0:1000], ptr_h_array_s[0:1000])
#pragma omp target map(ptr_h_array_h[:0], ptr_h_array_s[:0])
#pragma omp target
#pragma omp target exit data map(from:h_array_h[0:1000]) map(from:h_array_s[0:1000])
#pragma omp target map(ptr_h_array_h[0:1000], ptr_h_array_s[0:1000])
#pragma omp target map(ptr_h_array_h[:0], ptr_h_array_s[:0])
#pragma omp target
#pragma omp target enter data map(alloc: h_array_h[0:1000]) map(alloc: h_array_s[0:1000])
#pragma omp target exit data map(from: h_array_h[0:1000]) map(from: h_array_s[0:1000])
#pragma omp target map (from: _ompvv_isOffloadingOn)
