#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare target
#pragma omp taskgroup task_reduction(+: result)
#pragma omp task in_reduction(+: result)
#pragma omp end declare target
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target enter data map(to:temp[:1])
#pragma omp target data use_device_ptr(temp, next)
#pragma omp target parallel shared(result) num_threads(8) defaultmap(tofrom) map(root[:1])
#pragma omp single
#pragma omp target exit data map(release:temp[:1])
