#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target parallel num_threads(8) map(tofrom: a[:128 /*Array Size of 128 uses 16MB target memory and*/][:128 /*Array Size of 128 uses 16MB target memory and*/], b[:128 /*Array Size of 128 uses 16MB target memory and*/][:128 /*Array Size of 128 uses 16MB target memory and*/+1])
#pragma omp loop collapse(1)
#pragma omp target parallel num_threads(8) map(tofrom: a[:128 /*Array Size of 128 uses 16MB target memory and*/][:128 /*Array Size of 128 uses 16MB target memory and*/][:128 /*Array Size of 128 uses 16MB target memory and*/], b[:128 /*Array Size of 128 uses 16MB target memory and*/][:128 /*Array Size of 128 uses 16MB target memory and*/][:128 /*Array Size of 128 uses 16MB target memory and*/+1], num_threads)
#pragma omp loop collapse(2)
#pragma omp target map (from: _ompvv_isOffloadingOn)
