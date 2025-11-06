#pragma omp parallel for reduction(+: result) induction(step(x),*: xi)
#pragma omp parallel for reduction(+: result) reduction(inscan,*: xi)
#pragma omp scan exclusive(xi)
#pragma omp parallel for reduction(+: result) lastprivate(xi)
