#pragma omp declare mapper(custom: T v) map(to: v, v.len, v.data[0:v.len])
