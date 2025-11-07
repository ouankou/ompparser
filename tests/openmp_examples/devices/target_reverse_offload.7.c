#pragma omp requires reverse_offload
#pragma omp declare target device_type(host) enter(error_handler)
#pragma omp target map(A)
#pragma omp target device(ancestor: 1) map(always,to: A[i:1])
