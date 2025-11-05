#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(to: A[0:1024])
#pragma omp target data use_device_ptr(A)
#pragma omp target defaultmap(none) is_device_ptr(A) map(tofrom: B[0:1024])
#pragma omp task depend(out: B) shared(B) affinity(A[0:1024])
#pragma omp task depend(in: B) shared(B) affinity(A[0:1024])
#pragma omp taskwait
#pragma omp target map (from: _ompvv_isOffloadingOn)
