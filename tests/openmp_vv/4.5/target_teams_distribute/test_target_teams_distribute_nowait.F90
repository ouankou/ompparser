!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target enter data map(to: ticket(1:1), order(1:16     ), my_ticket)
!$omp        target teams distribute map(alloc: work_storage(1:1024, x),ticket(1:1)) private(my_ticket) nowait
!$omp           atomic capture
!$omp           end atomic
!$omp        end target teams distribute
!$omp     taskwait
!$omp     target exit data map(from:ticket(1:1), order(1:16     ))
