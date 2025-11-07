#pragma omp declare reduction(maxloc: struct mx_s) combiner( mx_combine(&omp_out, &omp_in) ) initializer( mx_init(&omp_priv, &omp_orig) )
#pragma omp parallel for reduction(maxloc: mx)
