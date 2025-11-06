#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target nowait depend(out: x[0:16384]) map(from: x[0:16384]) device(dev)
#pragma omp task depend(out: y[0:16384])
#pragma omp interop init(targetsync: obj) device(dev) depend(in: x[0:16384]) depend(inout: y[0:16384])
#pragma omp target depend(inout: y[0:16384]) depend(in: x[0:16384]) nowait map(to: x[0:16384]) map(tofrom: y[0:16384]) device(dev)
#pragma omp interop destroy(obj) nowait depend(out: y[0:16384])
#pragma omp target depend(inout: x[0:16384])
#pragma omp taskwait
