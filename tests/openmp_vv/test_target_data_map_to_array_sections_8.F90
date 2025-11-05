!$omp               target data map(to: my2DPtr(10:50-10, i)) map(to: my2DPtr2(10:, i), my2DPtr3(:50-10, i)) map(tofrom: myTmpArray)
