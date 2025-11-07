#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare variant( avx512_saxpy ) match( device={isa("core-avx512")} )
#pragma omp parallel for
#pragma omp parallel for simd simdlen(16) aligned(x,y:64)
