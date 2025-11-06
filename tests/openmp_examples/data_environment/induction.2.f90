!$omp   declare induction(next : (Point, real)) inductor(omp_var = omp_var%nextPoint(omp_step)) collector(omp_step * omp_idx)
!$omp   parallel do induction(step(Separation), next : P)
