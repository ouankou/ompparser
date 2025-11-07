!$omp   do ordered(2)
!$omp     ordered doacross(sink: j-1,i) doacross(sink: j,i-1)
!$omp     ordered doacross(source:)
