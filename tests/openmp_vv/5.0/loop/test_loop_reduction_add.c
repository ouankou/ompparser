#pragma omp parallel num_threads(8)
#pragma omp loop reduction(+:total)
#pragma omp for
