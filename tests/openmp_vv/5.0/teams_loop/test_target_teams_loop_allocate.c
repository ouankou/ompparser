#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams loop map(to: a[0:1024], b[0:1024]) map(from: c[0:1024]) uses_allocators(omp_low_lat_mem_alloc) allocate(omp_low_lat_mem_alloc: local) firstprivate(local)
#pragma omp target teams loop map(to: a[0:1024], b[0:1024]) map(from: c[0:1024]) uses_allocators(omp_default_mem_alloc) allocate(omp_default_mem_alloc: local) firstprivate(local)
#pragma omp target teams loop map(to: a[0:1024], b[0:1024]) map(from: c[0:1024]) uses_allocators(omp_large_cap_mem_alloc) allocate(omp_large_cap_mem_alloc: local) firstprivate(local)
#pragma omp target teams loop map(to: a[0:1024]) map(from: b[0:1024]) firstprivate(local) uses_allocators(omp_const_mem_alloc) allocate(omp_const_mem_alloc: local)
#pragma omp target teams loop map(to: a[0:1024], b[0:1024]) map(from: c[0:1024]) uses_allocators(omp_high_bw_mem_alloc) allocate(omp_high_bw_mem_alloc: local) firstprivate(local)
#pragma omp target teams loop map(to: a[0:1024], b[0:1024]) map(from: c[0:1024]) uses_allocators(omp_low_lat_mem_alloc) allocate(omp_low_lat_mem_alloc: local) firstprivate(local)
#pragma omp target teams loop map(to: a[0:1024], b[0:1024]) map(from: c[0:1024]) uses_allocators(omp_cgroup_mem_alloc) allocate(omp_cgroup_mem_alloc: local) firstprivate(local)
#pragma omp target teams loop map(to: a[0:1024], b[0:1024]) map(from: c[0:1024]) uses_allocators(omp_pteam_mem_alloc) allocate(omp_pteam_mem_alloc: local) firstprivate(local)
#pragma omp target map (from: _ompvv_isOffloadingOn)
