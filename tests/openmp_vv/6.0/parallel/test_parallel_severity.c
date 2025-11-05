#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel severity(warning) message("Entering first parallel region.")
#pragma omp parallel severity(warning) message("Entering second parallel region.")
