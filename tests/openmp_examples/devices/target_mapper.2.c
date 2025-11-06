#pragma omp declare mapper( top_id: dzmat_t v) map(v.r_m[0:100/2][0:100], v.i_m[0:100/2][0:100] )
#pragma omp declare mapper(bottom_id: dzmat_t v) map(v.r_m[100/2:100/2][0:100], v.i_m[100/2:100/2][0:100] )
#pragma omp target map(mapper(top_id), tofrom: a,b) device(0) firstprivate(is,ie) nowait
#pragma omp target map(mapper(bottom_id), tofrom: a,b) device(1) firstprivate(is,ie) nowait
#pragma omp taskwait
