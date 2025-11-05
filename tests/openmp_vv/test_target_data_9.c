#pragma omp target map(tofrom : a) nowait depend(out : a)
