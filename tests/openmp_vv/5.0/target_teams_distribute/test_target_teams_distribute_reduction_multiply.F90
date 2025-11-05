!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp        target teams distribute map(tofrom: device_result) reduction(*:device_result) map(to: a(1:1024))
