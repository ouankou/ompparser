!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     parallel do shared(pmax) private(oldval, assume, newval)
!$omp         atomic compare capture
!$omp         end atomic
!$omp     end parallel do
