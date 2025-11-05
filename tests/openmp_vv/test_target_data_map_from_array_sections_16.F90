!$omp             target data map(from: my1DArray(10:50-10)) map(from: my1DArray2(10:), my1DArray3(:50-10)) map(tofrom: myTmpArray)
