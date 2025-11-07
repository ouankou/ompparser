#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare target enter(do_more_work)
#pragma omp depobj(obj) depend(out: d_buf[0:N])
#pragma omp target is_device_ptr(d_buf) depend(depobj: obj)
