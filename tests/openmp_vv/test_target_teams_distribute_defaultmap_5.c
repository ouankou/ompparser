#pragma omp target teams distribute defaultmap(tofrom: scalar) map(tofrom: char_array[0:1024], short_array[0:1024], int_array[0:1024], float_array[0:1024], double_array[0:1024], enum_array[0:1024])
