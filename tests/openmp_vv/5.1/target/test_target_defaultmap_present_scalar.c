#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn) map(to: _ompvv_isSharedEnv)
#pragma omp target enter data map(to: scalar_var, float_var, double_var)
#pragma omp target map(tofrom: errors) defaultmap(present: scalar)
#pragma omp target exit data map(delete: scalar_var, float_var, double_var)
#pragma omp target map (from: _ompvv_isOffloadingOn)
