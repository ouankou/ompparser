#pragma omp parallel
#pragma omp for order(reproducible: concurrent) nowait
#pragma omp for order(reproducible: concurrent)
#pragma omp parallel
#pragma omp for schedule(static) order(concurrent) nowait
#pragma omp for schedule(static) order(concurrent)
#pragma omp parallel
#pragma omp for schedule(static) order(unconstrained: concurrent)
#pragma omp for schedule(static) order(unconstrained: concurrent)
