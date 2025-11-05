!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     declare target to(outer_fn)
!$omp       target map (tofrom: outcome, on_device)
!$omp       end target
