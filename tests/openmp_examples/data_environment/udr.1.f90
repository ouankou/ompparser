!$omp   declare reduction(min : point) combiner(minproc(omp_out, omp_in)) initializer(omp_priv = point(HUGE(0), HUGE(0)))
!$omp   declare reduction(max : point) combiner(maxproc(omp_out, omp_in)) initializer(omp_priv = point(0, 0))
!$omp   parallel do reduction(min:minp) reduction(max:maxp)
