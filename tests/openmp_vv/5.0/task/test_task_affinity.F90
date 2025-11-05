!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     task depend(out: B) shared(B) affinity(A)
!$omp     end task
!$omp     task depend(in: B) shared(B) affinity(A)
!$omp     end task
!$omp     taskwait
