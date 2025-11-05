#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map(tofrom:num_teams,errors)
#pragma omp teams num_teams(8) thread_limit(testing_thread_limit)
#pragma omp parallel
