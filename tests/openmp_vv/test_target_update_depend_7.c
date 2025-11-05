#pragma omp target update depend(in: in_1, in_2) depend(out: in_1, in_2) to(in_1[0:1000], in_2[0:1000])
