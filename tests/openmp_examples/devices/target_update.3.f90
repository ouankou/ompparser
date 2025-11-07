!$omp       declare mapper(custom: T :: v) map(to:v%x) map(from:v%y) map(alloc: v%z)
!$omp        target data map(mapper(custom),tofrom: s)
!$omp        target update to(mapper(custom) : s)
!$omp        target map(from: a,b,c)
!$omp        end target
!$omp        target update from(mapper(custom) : s)
!$omp        end target data
