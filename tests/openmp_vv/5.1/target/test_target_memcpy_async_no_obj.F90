!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     taskwait
!$omp     target is_device_ptr(mem_dev_cpy) device(t)
!$omp     teams distribute parallel do
!$omp     end teams distribute parallel do
!$omp     end target
!$omp     taskwait
