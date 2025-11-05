!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp              target if(s > 512      ) map(to: s, a(1:s), b(1:s)) map(tofrom: c(1:s), isHost)
!$omp              end target
