!$omp      declare target(d)
!$omp   target
!$omp   end target
!$omp   target map(b)
!$omp   end target
!$omp   target data map(c)
!$omp     target map(always,tofrom:c)
!$omp     end target
!$omp   end target data
!$omp   target map(always,tofrom:d)
!$omp   end target
