#pragma omp declare mapper(newvec_t v) map(v, v.data[0:v.len])
