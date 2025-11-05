#pragma omp target depend(out: dep_2) map(tofrom: dep_2[0:1000])
