#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target simd map(to: a[0:128 /*Array Size of 128 uses 16MB target memory and*/][0:128 /*Array Size of 128 uses 16MB target memory and*/]) map(tofrom: b[0:128 /*Array Size of 128 uses 16MB target memory and*/][0:128 /*Array Size of 128 uses 16MB target memory and*/+1]) collapse(1)
#pragma omp target simd map(to: a[0:128 /*Array Size of 128 uses 16MB target memory and*/][0:128 /*Array Size of 128 uses 16MB target memory and*/][0:128 /*Array Size of 128 uses 16MB target memory and*/]) map(tofrom: b[0:128 /*Array Size of 128 uses 16MB target memory and*/][0:128 /*Array Size of 128 uses 16MB target memory and*/][0:128 /*Array Size of 128 uses 16MB target memory and*/+1], num_teams) collapse(2)
