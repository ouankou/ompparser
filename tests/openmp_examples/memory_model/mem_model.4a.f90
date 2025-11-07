!$omp   parallel private(myid,tmp) num_threads(2)
!$omp       atomic write
!$omp       flush(b)
!$omp       flush(a)
!$omp       atomic read
!$omp       atomic write
!$omp       flush(a)
!$omp       flush(b)
!$omp       atomic read
!$omp   end parallel
