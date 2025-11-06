#pragma omp declare mapper(myvec_t v) map(v, v.data[0:v.len])
#pragma omp declare mapper(mypoints_t v) map(v.x, v.x[0] ) map(alloc:v.scratch)
#pragma omp target map(P)
