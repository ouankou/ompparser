#pragma omp target depend(in: dep_1) depend(in: dep_2) map(tofrom: dep_1[0:1000]) map(tofrom: dep_2[0:1000])
