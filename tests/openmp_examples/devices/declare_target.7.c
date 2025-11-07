#pragma omp declare target enter(foo) device_type(nohost)
#pragma omp declare variant(foo_onhost) match(device={kind(host)})
#pragma omp target teams
