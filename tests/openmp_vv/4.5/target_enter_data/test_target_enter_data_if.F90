!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp              target enter data if(s > 512      ) map(to: a(1:s), b(1:s))
!$omp              target map(to: a(1:s), b(1:s)) map(tofrom: c(1:s)) map(tofrom: s)
!$omp              end target
!$omp              target exit data if(s > 512      ) map(delete: a(1:s), b(1:s))
