!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp              target data if(s > 512      ) map(to: a(1:s), b(1:s))map(tofrom: c(1:s)) map(tofrom: isHost, s)
!$omp                target map(alloc: a(1:s), b(1:s), c(1:s)) map(tofrom: isHost, s)
!$omp                end target
!$omp              end target data
!$omp              target data if(s > 512      ) map(to: a(1:s), b(1:s))map(tofrom: c(1:s))
!$omp                target if(s > 512      ) map(alloc: a(1:s), b(1:s),c(1:s)) map(tofrom: isHost, s)
!$omp                end target
!$omp              end target data
