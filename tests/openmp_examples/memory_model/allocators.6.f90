!$omp      declare target(calc)
!$omp     requires dynamic_allocators
!$omp     declare target(cgroup_alloc)
!$omp      target teams reduction(+:xbuf) thread_limit(N) allocate(omp_cgroup_mem_alloc:xbuf) num_teams(4)
!$omp         parallel do
!$omp     end target teams
!$omp     parallel do reduction(+:sum)
!$omp     target
!$omp     end target
!$omp      target
!$omp      teams reduction(+:xbuf) thread_limit(N) allocate(cgroup_alloc:xbuf) num_teams(4)
!$omp         parallel do
!$omp     end teams
!$omp     end target
!$omp     target
!$omp     end target
!$omp     parallel do reduction(+:sum)
