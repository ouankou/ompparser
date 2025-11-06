#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp requires dynamic_allocators
#pragma omp declare target(calc)
#pragma omp declare target(cgroup_alloc)
#pragma omp target teams reduction(+:xbuf) thread_limit(256) allocate(omp_cgroup_mem_alloc:xbuf) num_teams(4)
#pragma omp parallel for
#pragma omp parallel for reduction(+:sum)
#pragma omp target
#pragma omp target
#pragma omp teams reduction(+:xbuf) thread_limit(256) allocate(cgroup_alloc:xbuf) num_teams(4)
#pragma omp parallel for
#pragma omp target
#pragma omp parallel for reduction(+:sum)
