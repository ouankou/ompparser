#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target update to(scalar_var, A, new_struct)
#pragma omp target enter data map(alloc: scalar_var, A, new_struct)
#pragma omp target update to(present: scalar_var, A, new_struct)
#pragma omp target map(tofrom: errors) defaultmap(none) map(to: scalar_var, A, new_struct)
#pragma omp target exit data map(release: scalar_var, A, new_struct)
