#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(to: new_struct.data[:1024])
#pragma omp target map(to: new_struct, new_struct.data[:1024]) map(tofrom: A[:1024])
#pragma omp target update to(iterator(it = 0:1024): new_struct.data[it])
#pragma omp target map(tofrom: A[:1024])
#pragma omp target exit data map(delete:new_struct.data[:1024])
#pragma omp target map (from: _ompvv_isOffloadingOn)
