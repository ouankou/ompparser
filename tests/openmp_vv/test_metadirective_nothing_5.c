#pragma omp target map(tofrom: A, thread_limit_teams) thread_limit(thread_limit_target)
