#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams distribute parallel for num_teams(2) thread_limit(10) default(firstprivate) shared(ErrCount) map(tofrom: ErrCount)
#pragma omp atomic
#pragma omp target teams distribute parallel for num_teams(2) thread_limit(10) default(private) shared(ErrCount) firstprivate(Arr) map(to: Arr[0:32]) map(tofrom: ErrCount)
#pragma omp atomic
#pragma omp target map (from: _ompvv_isOffloadingOn)
