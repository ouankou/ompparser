#pragma omp target map(to: B[0:N], C[0:N]) map(tofrom: sum)
#pragma omp teams num_teams(num_teams) thread_limit(block_threads) reduction(+:sum)
#pragma omp distribute
#pragma omp parallel for reduction(+:sum)
