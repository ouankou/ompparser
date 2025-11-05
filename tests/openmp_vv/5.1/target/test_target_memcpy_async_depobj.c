#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp depobj(obj) depend(inout: mem_dev_cpy)
#pragma omp taskwait depend(depobj: obj)
#pragma omp target is_device_ptr(mem_dev_cpy) device(t) depend(depobj: obj)
#pragma omp taskwait depend(depobj: obj)
#pragma omp depobj(obj) destroy
#pragma omp target map (from: _ompvv_isOffloadingOn)
