#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(tofrom: A)
#pragma omp parallel num_threads(4)
#pragma omp metadirective when( device={arch("nvptx")}: masked ) when( implementation={vendor(amd)}: masked ) when (implementation={vendor(nvidia)}: masked) when( device={kind(nohost)}: masked ) default( for)
#pragma omp target map (from: _ompvv_isOffloadingOn)
