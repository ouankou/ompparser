#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp begin declare target
#pragma omp end declare target
#pragma omp allocate(v1,v2) allocator(omp_high_bw_mem_alloc)
#pragma omp allocate(v3,v4) allocator(omp_default_mem_alloc)
#pragma omp target map(to: v3[0:1024*1024*64 /* big memory, default*/], v4[0:1024*1024*64 /* big memory, default*/]) map(from:v3[0:1024*1024*64 /* big memory, default*/])
#pragma omp task private(v5,v6) allocate(allocator(omp_low_lat_mem_alloc): v5,v6)
