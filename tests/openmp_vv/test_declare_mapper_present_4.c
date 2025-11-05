#pragma omp declare mapper(default:myvec_t v) map(always, present, from: v, v.len, v.data[0:v.len])
