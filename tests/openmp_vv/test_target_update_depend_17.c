#pragma omp target update depend(inout: in_1, in_2) to(in_1[0:1000], in_2[0:1000])
