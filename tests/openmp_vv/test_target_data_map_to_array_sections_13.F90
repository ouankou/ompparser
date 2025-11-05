!$omp                   target map(alloc: my3DPtr(10:50-10, i, j)) map(alloc: my3DPtr2(10:, i, j), my3DPtr3(:50-10, i, j)) map(tofrom: myTmpArray)
