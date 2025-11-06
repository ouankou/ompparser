#pragma omp begin declare target
#pragma omp declare simd uniform(i) linear(k) notinbranch
#pragma omp end declare target
#pragma omp target map(tofrom: tmp)
#pragma omp parallel for reduction(+:tmp)
#pragma omp simd reduction(+:tmp1)
