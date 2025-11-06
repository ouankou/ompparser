!$omp   declare reduction( min : point ) combiner(omp_out = point(min(omp_out%x, omp_in%x), min(omp_out%y, omp_in%y))) initializer(omp_priv = point(HUGE(0), HUGE(0)))
!$omp   declare reduction( max : point ) combiner(omp_out = point(max(omp_out%x, omp_in%x), max(omp_out%y, omp_in%y))) initializer(omp_priv = point(0, 0))
!$omp   parallel do reduction(min: minp) reduction(max: maxp)
