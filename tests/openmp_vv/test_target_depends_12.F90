!$omp             target depend(in: dep_1) depend(in: dep_2) map(tofrom:dep_1(1:1024)) map(tofrom: dep_2(1:1024))
