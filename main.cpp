#include <stdio.h>
#include <OpenMPIR.h>
#include <string.h>
#include <iostream>

extern OpenMPDirective* parseOpenMP(const char*, void * _exprParse(const char*));

void output(OpenMPDirective*);

void output(OpenMPDirective* node) {
    
    std::string unparsing_string = node->generatePragmaString();
    std::cout << unparsing_string << "\n";
    node->generateDOT();

}

int main( int argc, const char* argv[] ) {

    //const char* input = "omp parallel private (a[foo(x, goo(x, y)):100], b[1:30], c) num_threads (3*5+4/(7+10)) firstprivate (foo(x), y), shared (a, b, c[1:10]), copyin (a[foo(goo(x)):20],a,y) default (none) if (parallel : b) proc_bind (master) reduction (inscan, + : a, foo(x)) reduction (abc : x, y, z) allocate (omp_high_bw_mem_alloc : m, n[1:5]) allocate (no, allo, cator)";

<<<<<<< HEAD
	//const char* input = "omp parallel private (a[foo(x, goo(x, y)):100], b[1:30], c) firstprivate (foo(x), y), shared (a, b, c[1:10]) ";
=======
	const char* input = "OMP parallel private (a[foo(x, goo(x, y)):100], b[1:30], c) firstprivate (foo(x), y), shared (a, b, c[1:10]) ";
>>>>>>> 08933e9f8f4560f67576979c19bbfb66841666e5

	//const char* input = "omp parallel private (a[foo(x, goo(x, y)):100], b[1:30], c) num_threads (3*5+4/(7+10)) allocate (no, allo, cator)";
	
        const char* input = "omp for collapse(a) order(dasfe) nowait ordered(sd)";
   

    OpenMPDirective* openMPAST = parseOpenMP(input, NULL);
    
    output(openMPAST);
    printf("\n");


    return 0;
}

