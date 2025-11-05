#pragma omp target parallel for firstprivate(HostVar) device(gpu)
