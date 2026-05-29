#pragma omp parallel num_threads(8)
#pragma omp loop reduction(||:result)
#pragma omp for
