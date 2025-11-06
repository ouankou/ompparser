!$omp   do ordered(1)
!$omp   ordered doacross(sink: i-1)
!$omp   ordered doacross(source: omp_cur_iteration)
