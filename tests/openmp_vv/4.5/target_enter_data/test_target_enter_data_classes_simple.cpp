#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(to: helper[0:hs]) map(to: hs) map(to:hsum)
#pragma omp target exit data map(delete: helper[0:hs]) map(delete: hs) map(delete: hsum)
#pragma omp target map(alloc:hsum, hsize)
#pragma omp target map(from: h_array[0:hsize]) map(alloc: help_sum, hsize) map(from:h_sum)
#pragma omp target map (from: _ompvv_isOffloadingOn)
