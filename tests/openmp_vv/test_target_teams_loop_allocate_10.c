#pragma omp target teams loop map(to: a[0:1024], b[0:1024]) map(from: c[0:1024]) uses_allocators(omp_cgroup_mem_alloc) allocate(omp_cgroup_mem_alloc: local) firstprivate(local)
