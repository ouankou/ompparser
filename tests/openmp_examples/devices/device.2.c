#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target if(do_offload) map(to: v1[0:N], v2[:N]) map(from: p[0:N])
#pragma omp parallel for if(N>1000) private(i)
