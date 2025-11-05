#pragma omp target map(iterator(it = 0:1024), tofrom: test_lst[it][:1]) map(to: test_lst)
