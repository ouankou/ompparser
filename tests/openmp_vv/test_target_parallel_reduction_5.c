#pragma omp target parallel for reduction(+:TotSum) device(gpu)
