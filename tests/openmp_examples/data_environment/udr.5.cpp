#pragma omp declare reduction( + : V ) combiner( omp_out += omp_in ) initializer(omp_priv(omp_orig))
