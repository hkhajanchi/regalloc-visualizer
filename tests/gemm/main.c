#include "stdlib.h"
#include "stdio.h"
#include "time.h"

#define ROWS 10
#define COLS 10

// Computes a square-matrix GEMM using random values
void gemm(int rows, int cols) {

  int* a = (int*)malloc(sizeof(int)*rows*cols);
  int* b = (int*)malloc(sizeof(int)*rows*cols);
  int* c = (int*)malloc(sizeof(int)*rows*cols);

  // use ptr aritmetic for array indexing
  // initialize both arrays to random values
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      *(a + i*rows + j) = rand();
      *(b + i*rows + j) = rand();
    }
  }

  int sum = 0;
  // this is probably wrong but hopefully it doesn't segfault
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      sum = 0;

      for (int k = 0; k < cols; k++) {

        sum += *(a + i*cols +k) * *(b + k*rows + j);

      }
      *(c + i*rows + j) = sum;
            

      }

    }
  }


int main() {

  printf("Computing GEMM of %i x %i square matrices \n", ROWS, COLS);
  
  clock_t clk = clock();
  gemm(ROWS,COLS);
  clk = clock() - clk;
  printf("Runtime: %f seconds. \n", (double)clk/CLOCKS_PER_SEC);


}



