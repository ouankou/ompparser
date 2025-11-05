#pragma omp target teams distribute map(tofrom: char_array_a[0:1024], short_array_a[0:1024], int_array_a[0:1024], float_array_a[0:1024], double_array_a[0:1024], enum_array_a[0:1024])
