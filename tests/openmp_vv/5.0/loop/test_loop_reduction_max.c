#pragma omp parallel num_threads(8)
#pragma omp loop reduction(max:result)
#pragma omp for
