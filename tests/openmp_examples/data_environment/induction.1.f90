!$omp    parallel do reduction(+: result) induction(step(x),*: xi)
!$omp    parallel do reduction(+: result) reduction(inscan,*: xi)
!$omp       scan exclusive(xi)
!$omp    parallel do reduction(+: result) lastprivate(xi)
