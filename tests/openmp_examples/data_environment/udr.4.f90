!$omp declare reduction(.add. : dt) combiner(omp_out=omp_out.add.omp_in)initializer(dt_init(omp_priv))
!$omp parallel do reduction(.add.: xdt1)
!$omp end parallel do
