!$omp    declare target enter(do_more_work)
!$omp    depobj(obj) depend(out: d_buf(1:N))
!$omp    target has_device_addr(d_buf) depend(depobj: obj)
!$omp    end target
