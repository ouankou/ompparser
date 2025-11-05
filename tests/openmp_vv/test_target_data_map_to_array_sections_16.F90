!$omp             target data map(to: my1DArray(10:50-10)) map(to: my1DArray2(10:), my1DArray3(:50-10)) map(tofrom: myTmpArray)
