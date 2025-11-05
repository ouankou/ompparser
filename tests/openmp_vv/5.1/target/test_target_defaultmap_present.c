#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(to: scalar_var, A, new_struct, ptr)
#pragma omp target map(tofrom: errors) defaultmap(present)
#pragma omp target exit data map(delete: scalar_var, A, new_struct, ptr)
#pragma omp target map (from: _ompvv_isOffloadingOn)
