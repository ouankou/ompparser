!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     depobj(obj) depend(inout: mem_dev_cpy)
!$omp     taskwait depend(depobj: obj)
!$omp     target is_device_ptr(mem_dev_cpy) device(t) depend(depobj: obj)
!$omp     end target
!$omp     taskwait depend(depobj: obj)
!$omp     depobj(obj) destroy
