#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(to:v1,v2) map(from:v3, target_device_num) device(default_device)
#pragma omp metadirective when( device={arch("nvptx")}: teams distribute parallel for) default( parallel for)
#pragma omp atomic write
#pragma omp target map (from: _ompvv_isOffloadingOn)
