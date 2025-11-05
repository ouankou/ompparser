#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(alloc: A)
#pragma omp metadirective when( target_device={device_num(dev)}: target defaultmap(none) map(always,tofrom: A)) default( target defaultmap(none) map(to: A))
#pragma omp target exit data map(release: A)
#pragma omp target map (from: _ompvv_isOffloadingOn)
