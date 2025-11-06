#pragma omp declare reduction(min : struct point) combiner( omp_out.x = omp_in.x > omp_out.x ? omp_out.x : omp_in.x, omp_out.y = omp_in.y > omp_out.y ? omp_out.y : omp_in.y ) initializer( omp_priv = { 2147483647, 2147483647 } )
#pragma omp declare reduction(max : struct point) combiner( omp_out.x = omp_in.x < omp_out.x ? omp_out.x : omp_in.x, omp_out.y = omp_in.y < omp_out.y ? omp_out.y : omp_in.y ) initializer( omp_priv = { 0, 0 } )
#pragma omp parallel for reduction(min:minp) reduction(max:maxp)
