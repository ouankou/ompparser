#pragma omp parallel num_threads(8)
#pragma omp loop reduction(min:result)
#pragma omp for
