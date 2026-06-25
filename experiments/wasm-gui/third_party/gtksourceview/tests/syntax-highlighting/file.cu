#include "cuMatrix.h"

__global__ void make_BlackWhite(int *image, int N){
	unsigned int x = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int y = blockIdx.y * blockDim.y + threadIdx.y;

	image[y*N + x] = image[y*N + x] > 128 ? 255 : 0;
}

void convertToArray(int **matrix, int *array, int N){
	for(unsigned int i=0; i< N; i++)
		for(unsigned int j=0; j< N; j++)
			array[i*N+ j] = matrix[i][j];
}

// Wrapper function for kernel launch (not the complete function, just an example).
template <class T>
void
reduce(int size, int threads, int blocks,
       int whichKernel, T *d_idata, T *d_odata)
{
	dim3 dimBlock(threads, 1, 1);
	dim3 dimGrid(blocks, 1, 1);

	// when there is only one warp per block, we need to allocate two warps
	// worth of shared memory so that we don't index shared memory out of bounds
	int smemSize = (threads <= 32) ? 2 * threads * sizeof(T) : threads * sizeof(T);

	// choose which of the optimized versions of reduction to launch
	switch (whichKernel)
	{
		case 0:
			reduce0<T><<< dimGrid, dimBlock, smemSize >>>(d_idata, d_odata, size);
			break;

		case 1:
			reduce1<T><<< dimGrid, dimBlock, smemSize >>>(d_idata, d_odata, size);
			break;
	}
}
