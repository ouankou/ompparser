#pragma omp taskwait nowait depend(inout: test_scalar) depend(out: test_arr)
