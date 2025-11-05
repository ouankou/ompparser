#pragma omp target depend(inout: A[0:1024]) nowait map(tofrom: A[0:1024]) device(device)
