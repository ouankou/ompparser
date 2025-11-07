!$omp    declare reduction(maxloc: mx_s) combiner(mx_combine(omp_out, omp_in)) initializer(mx_init(omp_priv, omp_orig))
!$omp    parallel do reduction(maxloc: mx)
