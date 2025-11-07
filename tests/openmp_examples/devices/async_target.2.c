#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp begin declare target
#pragma omp end declare target
#pragma omp task shared(v1, v2) depend(out: v1, v2)
#pragma omp target device(dev) map(v1, v2)
#pragma omp task shared(v1, v2, p) depend(in: v1, v2)
#pragma omp target device(dev) map(to: v1, v2) map(from: p[0:N])
#pragma omp parallel for
#pragma omp taskwait
