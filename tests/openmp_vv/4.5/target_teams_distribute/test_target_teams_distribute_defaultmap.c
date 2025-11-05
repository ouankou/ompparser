#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams distribute defaultmap(tofrom: scalar) map(from: char_array[0:1024], short_array[0:1024], int_array[0:1024], float_array[0:1024], double_array[0:1024], enum_array[0:1024])
#pragma omp target teams distribute defaultmap(tofrom: scalar) map(tofrom: char_array[0:1024], short_array[0:1024], int_array[0:1024], float_array[0:1024], double_array[0:1024], enum_array[0:1024])
#pragma omp target teams distribute map(tofrom: char_array_a[0:1024], char_array_b[0:1024], short_array_a[0:1024], short_array_b[0:1024], int_array_a[0:1024], int_array_b[0:1024], float_array_a[0:1024], float_array_b[0:1024], double_array_a[0:1024], double_array_b[0:1024], enum_array_a[0:1024], enum_array_b[0:1024])
#pragma omp target teams distribute map(tofrom: char_array_a[0:1024], short_array_a[0:1024], int_array_a[0:1024], float_array_a[0:1024], double_array_a[0:1024], enum_array_a[0:1024])
#pragma omp target teams distribute
#pragma omp target map (from: _ompvv_isOffloadingOn)
