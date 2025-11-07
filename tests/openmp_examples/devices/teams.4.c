#pragma omp target map(to: B[0:1024*1024], C[0:1024*1024]) map(tofrom: sum)
#pragma omp teams num_teams(8) thread_limit(16) reduction(+:sum)
#pragma omp distribute parallel for reduction(+:sum) dist_schedule(static, 1024) schedule(static, 64)
