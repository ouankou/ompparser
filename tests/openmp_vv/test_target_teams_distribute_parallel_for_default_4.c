#pragma omp target teams distribute parallel for num_teams(2) thread_limit(10) default(firstprivate) shared(ErrCount) map(tofrom: ErrCount)
