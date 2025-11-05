#pragma omp target data map(tofrom: A[0:1024], InitDev[0:1]) if(Proc) device(gpu)
