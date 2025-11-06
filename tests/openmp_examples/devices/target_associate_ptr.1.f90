!$omp     target update to(arr(ioff:ioff+CS-1)) device(dev)
!$omp     target map(tofrom: arr(ioff:ioff+CS-1)) device(dev)
!$omp     end target
!$omp     target update from(arr(ioff:ioff+CS-1)) device(dev)
