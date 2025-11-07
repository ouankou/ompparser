!$omp    declare target
!$omp       task shared(z)
!$omp       target map(z(C:C+CHUNKSZ-1))
!$omp       parallel do
!$omp       end target
!$omp       end task
!$omp    taskwait
