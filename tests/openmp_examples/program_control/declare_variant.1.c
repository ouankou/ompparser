#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare variant( p_vxv ) match( construct={parallel} )
#pragma omp declare variant( t_vxv ) match( construct={target} )
#pragma omp for
#pragma omp begin declare target
#pragma omp distribute simd
#pragma omp end declare target
#pragma omp parallel
#pragma omp target teams map(to: v1[:100],v2[:100]) map(from: v3[:100])
