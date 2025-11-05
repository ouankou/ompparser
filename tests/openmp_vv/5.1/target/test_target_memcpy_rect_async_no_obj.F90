!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     taskwait
!$omp     target is_device_ptr(devRect) device(t)
!$omp     end target
!$omp     taskwait
