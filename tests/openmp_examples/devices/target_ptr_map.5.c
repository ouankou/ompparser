#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare mapper(deep_copy: T s) map(s, s.ptr[:s.buf_size])
#pragma omp metadirective when(user={condition(accessible)}: target) otherwise(target map(mapper(deep_copy),tofrom:s) )
