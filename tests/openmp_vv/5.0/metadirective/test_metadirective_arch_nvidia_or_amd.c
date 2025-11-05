#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target device(device_num) map(from:initial_device)
#pragma omp metadirective when( implementation={vendor(nvidia)}: teams num_teams(512) thread_limit(32) ) when( implementation={vendor(amd)}: teams num_teams(512) thread_limit(64) ) default (teams)
#pragma omp distribute parallel for
#pragma omp atomic write
#pragma omp target map (from: _ompvv_isOffloadingOn)
