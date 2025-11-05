#pragma omp target exit data map(release: h_array[0:1000], in_1[0:1000], in_2[0:1000])
