#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target update to(arr[ioff:50]) device(dev)
#pragma omp target map(tofrom : arr[ioff:50]) device(dev)
#pragma omp target update from(arr[ioff:50]) device(dev)
