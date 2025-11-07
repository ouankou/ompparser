#pragma omp declare induction(next : (Point, float)) inductor (omp_var = omp_var.nextPoint(omp_step)) collector(omp_step * omp_idx)
#pragma omp parallel for induction(step(Separation), next : P)
