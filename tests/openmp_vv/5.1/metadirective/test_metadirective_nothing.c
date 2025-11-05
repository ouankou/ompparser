#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(from: thread_limit_target) thread_limit(thread_limit_target)
#pragma omp target map(tofrom: A, thread_limit_teams) thread_limit(thread_limit_target)
#pragma omp metadirective when( device={kind(nohost)}: nothing ) when( device={arch("nvptx")}: nothing) when( implementation={vendor(amd)}: nothing ) default( teams distribute parallel for thread_limit(thread_limit_target+1) )
#pragma omp metadirective when( device={kind(nohost)}: teams distribute parallel for thread_limit(thread_limit_initial+1) ) when( device={arch("nvptx")}: teams distribute parallel for thread_limit(thread_limit_initial+1) ) when( implementation={vendor(amd)}: teams distribute parallel for thread_limit(thread_limit_initial+1) ) default( nothing )
#pragma omp target map (from: _ompvv_isOffloadingOn)
