#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel
#pragma omp single
#pragma omp task depend(inout: test_scalar) shared(test_scalar)
#pragma omp taskwait nowait depend(inout: test_scalar) depend(out: test_arr)
#pragma omp task depend(inout : test_arr) shared(test_arr)
#pragma omp target map (from: _ompvv_isOffloadingOn)
