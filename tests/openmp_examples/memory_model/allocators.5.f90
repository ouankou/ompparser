!$omp      declare target(calc)
!$omp     target uses_allocators(omp_cgroup_mem_alloc)
!$omp     teams  reduction(+:xbuf) thread_limit(N) allocate(omp_cgroup_mem_alloc:xbuf) num_teams(4)
!$omp         parallel do
!$omp     end teams
!$omp     end target
!$omp     parallel do reduction(+:sum)
!$omp      target uses_allocators(traits(cgroup_traits): cgroup_alloc)
!$omp      teams  reduction(+:xbuf) thread_limit(N) allocate(cgroup_alloc:xbuf) num_teams(4)
!$omp         parallel do
!$omp     end teams
!$omp     end target
!$omp     parallel do reduction(+:sum)
!$omp      target uses_allocators(cgroup_alloc)
!$omp      teams  reduction(+:xbuf) thread_limit(N) allocate(cgroup_alloc:xbuf) num_teams(4)
!$omp         parallel do
!$omp     end teams
!$omp     end target
!$omp     parallel do reduction(+:sum)
