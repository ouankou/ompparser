#pragma omp declare reduction(min : struct point) combiner( minproc(&omp_out, &omp_in) ) initializer( omp_priv = { 2147483647, 2147483647 } )
#pragma omp declare reduction(max : struct point) combiner( maxproc(&omp_out, &omp_in) ) initializer( omp_priv = { 0, 0 } )
#pragma omp parallel for reduction(min:minp) reduction(max:maxp)
