!$omp   parallel private(myid,tmp) num_threads(2)
!$omp       atomic write
!$omp       flush(a,b)
!$omp       atomic read
!$omp       atomic write
!$omp       flush(a,b)
!$omp       atomic read
!$omp   end parallel
