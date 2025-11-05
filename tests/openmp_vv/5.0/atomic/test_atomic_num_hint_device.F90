!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target map(tofrom: num_threads,x)
!$omp     parallel num_threads(8                       ) default(shared)
!$omp        atomic hint(4)
!$omp        end atomic
!$omp     end parallel
!$omp     end target
!$omp     target map(tofrom: num_threads,x)
!$omp     parallel num_threads(8                       ) default(shared)
!$omp        atomic hint(4132)
!$omp        end atomic
!$omp     end parallel
!$omp     end  target
