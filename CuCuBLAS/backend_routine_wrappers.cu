///
/// \author Anastasiadis Petros (panastas@cslab.ece.ntua.gr)
///
/// \brief Backened wrapped host functions for cuda queue firing -> void func (void*)
///

#include <cblas.h>
#include <omp.h>

#include "backend_wrappers.hpp"

void CoCoQueueLock(void* wrapped_lock){
#ifdef ENABLE_MUTEX_LOCKING
  (*(std::mutex*)wrapped_lock).lock();
#else
  while(__sync_lock_test_and_set ((&(*((int*)wrapped_lock))), 1));
#endif
#ifdef DEBUG
  fprintf(stderr, "CoCoQueueLock(%p) ran succesfully.\n", wrapped_lock);
#endif
}

void CoCoQueueUnlock(void* wrapped_lock){
#ifdef ENABLE_MUTEX_LOCKING
	(*(std::mutex*)wrapped_lock).unlock();
#else
  //int* intptr = (int*) wrapped_lock;
  //*intptr = 0;
  __sync_lock_release((&(*((int*) wrapped_lock))));
#endif

#ifdef DEBUG
  fprintf(stderr, "CoCoQueueUnlock(%p) ran succesfully.\n", wrapped_lock);
#endif
}

void CoCoIncAsync(void* wrapped_ptr_int){
  Ptr_atomic_int_p unwrapped = (Ptr_atomic_int_p) wrapped_ptr_int;
  *(unwrapped->ato_int_ptr)++;
  free(unwrapped);
#ifdef DEBUG
  fprintf(stderr, "CoCoIncAsync(%p, new_val=%d) ran succesfully.\n", unwrapped->ato_int_ptr, (*(unwrapped->ato_int_ptr)).load());
#endif
}

void CoCoDecAsync(void* wrapped_ptr_int){
  Ptr_atomic_int_p unwrapped = (Ptr_atomic_int_p) wrapped_ptr_int;
  (*(unwrapped->ato_int_ptr))--;
  free(unwrapped);
#ifdef DEBUG
  fprintf(stderr, "CoCoDecAsync(%p, new_val=%d) ran succesfully.\n", unwrapped->ato_int_ptr, (*(unwrapped->ato_int_ptr)).load());
#endif
}

void CoCoSetInt(void* wrapped_ptr_and_val){
  Ptr_and_int_p unwrapped = (Ptr_and_int_p) wrapped_ptr_and_val;
  *(unwrapped->int_ptr) = unwrapped->val;
  free(unwrapped);
#ifdef DEBUG
  fprintf(stderr, "CoCoSetVal(%p, %d) ran succesfully.\n", unwrapped->int_ptr, unwrapped->val);
#endif
}

void CoCoSetPtr(void* wrapped_ptr_and_parent){
  Ptr_and_parent_p unwrapped = (Ptr_and_parent_p) wrapped_ptr_and_parent;
  void* prev_ptr = *(unwrapped->ptr_parent);
  *(unwrapped->ptr_parent) = unwrapped->ptr_val;
  free(unwrapped);
#ifdef DEBUG
  fprintf(stderr, "CoCoSetPtr(prev=%p, %p) ran succesfully.\n", prev_ptr, unwrapped->ptr_val);
#endif
}

void cblas_wrap_daxpy(void* backend_data){
  axpy_backend_in<double>* ptr_ker_translate = (axpy_backend_in<double>*) backend_data;
#ifdef DEBUG
  fprintf(stderr,"cblas_wrap_daxpy: cublasDaxpy(dev_id = %d,\
    N = %d, alpha = %lf, x = %p, incx = %d, y = %p, incy = %d)\n",
    ptr_ker_translate->dev_id, ptr_ker_translate->N, ptr_ker_translate->alpha,
    (double*) *ptr_ker_translate->x, ptr_ker_translate->incx,
    (double*) *ptr_ker_translate->y, ptr_ker_translate->incy);
#endif
  cblas_daxpy(ptr_ker_translate->N, ptr_ker_translate->alpha,
    (double*) *ptr_ker_translate->x, ptr_ker_translate->incx, (double*)
    *ptr_ker_translate->y, ptr_ker_translate->incy);
}

void cblas_wrap_daxpby(void* backend_data){
  axpby_backend_in<double>* ptr_ker_translate = (axpby_backend_in<double>*) backend_data;
#ifdef DEBUG
  fprintf(stderr,"cblas_wrap_daxpby: cblas_daxpby(dev_id = %d,\
    N = %d, alpha = %lf, x = %p, incx = %d, b = %lf, y = %p, incy = %d)\n",
    ptr_ker_translate->dev_id, ptr_ker_translate->N, ptr_ker_translate->alpha,
    (double*) *ptr_ker_translate->x, ptr_ker_translate->incx, ptr_ker_translate->beta,
    (double*) *ptr_ker_translate->y, ptr_ker_translate->incy);
#endif
  cblas_daxpby(ptr_ker_translate->N, ptr_ker_translate->alpha,
    (double*) *ptr_ker_translate->x, ptr_ker_translate->incx, ptr_ker_translate->beta,
    (double*) *ptr_ker_translate->y, ptr_ker_translate->incy);
}

void cblas_wrap_saxpy(void* backend_data){
  axpy_backend_in<float>* ptr_ker_translate = (axpy_backend_in<float>*) backend_data;
  cblas_saxpy(ptr_ker_translate->N, ptr_ker_translate->alpha,
    (float*) *ptr_ker_translate->x, ptr_ker_translate->incx, (float*)
    *ptr_ker_translate->y, ptr_ker_translate->incy);
}

void cblas_wrap_ddot(void* backend_data){
  dot_backend_in<double>* ptr_ker_translate = (dot_backend_in<double>*) backend_data;
  *ptr_ker_translate->result = cblas_ddot(ptr_ker_translate->N, (double*) *ptr_ker_translate->x,
  ptr_ker_translate->incx, (double*) *ptr_ker_translate->y,
  ptr_ker_translate->incy);
}

void cblas_wrap_dgemm(void* backend_data){
  gemm_backend_in<double>* ptr_ker_translate = (gemm_backend_in<double>*) backend_data;
#ifdef DDEBUG
  if (ptr_ker_translate->dev_id != -1)
    warning("cblas_wrap_dgemm: Suspicious device %d instead of -1\n", ptr_ker_translate->dev_id);
#endif
#ifdef DDEBUG
  fprintf(stderr,"cblas_wrap_dgemm: cblas_dgemm(dev_id = %d, TransA = %c, TransB = %c,\
    M = %d, N = %d, K = %d, alpha = %lf, A = %p, lda = %d, \n\
    B = %p, ldb = %d, beta = %lf, C = %p, ldC = %d)\n",
    ptr_ker_translate->dev_id, ptr_ker_translate->TransA, ptr_ker_translate->TransB,
    ptr_ker_translate->M, ptr_ker_translate->N, ptr_ker_translate->K, ptr_ker_translate->alpha,
    (double*) *ptr_ker_translate->A, ptr_ker_translate->ldA,
    (double*) *ptr_ker_translate->B, ptr_ker_translate->ldB,
    ptr_ker_translate->beta, (double*) *ptr_ker_translate->C, ptr_ker_translate->ldC);
#endif
  cblas_dgemm(CblasColMajor,
    OpCharToCblas(ptr_ker_translate->TransA), OpCharToCblas(ptr_ker_translate->TransB),
    ptr_ker_translate->M, ptr_ker_translate->N, ptr_ker_translate->K, ptr_ker_translate->alpha,
    (double*) *ptr_ker_translate->A, ptr_ker_translate->ldA,
    (double*) *ptr_ker_translate->B, ptr_ker_translate->ldB,
    ptr_ker_translate->beta, (double*) *ptr_ker_translate->C, ptr_ker_translate->ldC);
}

void cblas_wrap_dgemv(void* backend_data){
  gemv_backend_in<double>* ptr_ker_translate = (gemv_backend_in<double>*) backend_data;
#ifdef DDEBUG
  if (ptr_ker_translate->dev_id != -1)
    warning("cblas_wrap_dgemv: Suspicious device %d instead of -1\n", ptr_ker_translate->dev_id);
#endif
#ifdef DDEBUG
  fprintf(stderr,"cblas_wrap_dgemv: cblas_dgemv(dev_id = %d, TransA = %c\
    M = %d, N = %d,alpha = %lf, A = %p, lda = %d, \n\
    beta = %lf, x = %p, incx = %d, y = %p, incy = %d)\n",
    ptr_ker_translate->dev_id, ptr_ker_translate->TransA,
    ptr_ker_translate->M, ptr_ker_translate->N, ptr_ker_translate->alpha,
    (double*) *ptr_ker_translate->A, ptr_ker_translate->ldA,
    (double*) *ptr_ker_translate->x, ptr_ker_translate->incx,
    ptr_ker_translate->beta, (double*) *ptr_ker_translate->y, ptr_ker_translate->incy);
#endif
  cblas_dgemv(CblasColMajor,
    OpCharToCblas(ptr_ker_translate->TransA),
    ptr_ker_translate->M, ptr_ker_translate->N, ptr_ker_translate->alpha,
    (double*) *ptr_ker_translate->A, ptr_ker_translate->ldA,
    (double*) *ptr_ker_translate->x, ptr_ker_translate->incx,
    ptr_ker_translate->beta, (double*) *ptr_ker_translate->y, ptr_ker_translate->incy);
}

void cblas_wrap_sgemm(void* backend_data){
  gemm_backend_in<float>* ptr_ker_translate = (gemm_backend_in<float>*) backend_data;
#ifdef DDEBUG
  if (ptr_ker_translate->dev_id != -1)
    warning("cblas_wrap_sgemm: Suspicious device %d instead of -1\n", ptr_ker_translate->dev_id);
#endif
#ifdef DDEBUG
  fprintf(stderr,"cblas_wrap_sgemm: cblas_dgemm(dev_id = %d, TransA = %c, TransB = %c,\
    M = %d, N = %d, K = %d, alpha = %lf, A = %p, lda = %d, \n\
    B = %p, ldb = %d, beta = %lf, C = %p, ldC = %d)\n",
    ptr_ker_translate->dev_id, ptr_ker_translate->TransA, ptr_ker_translate->TransB,
    ptr_ker_translate->M, ptr_ker_translate->N, ptr_ker_translate->K, ptr_ker_translate->alpha,
    (float*) *ptr_ker_translate->A, ptr_ker_translate->ldA,
    (float*) *ptr_ker_translate->B, ptr_ker_translate->ldB,
    ptr_ker_translate->beta, (float*) *ptr_ker_translate->C, ptr_ker_translate->ldC);
#endif
  cblas_sgemm(CblasColMajor,
    OpCharToCblas(ptr_ker_translate->TransA), OpCharToCblas(ptr_ker_translate->TransB),
    ptr_ker_translate->M, ptr_ker_translate->N, ptr_ker_translate->K, ptr_ker_translate->alpha,
    (float*) *ptr_ker_translate->A, ptr_ker_translate->ldA,
    (float*) *ptr_ker_translate->B, ptr_ker_translate->ldB,
    ptr_ker_translate->beta, (float*) *ptr_ker_translate->C, ptr_ker_translate->ldC);
}

void cublas_wrap_daxpy(void* backend_data, void* queue_wrap_p){
  axpy_backend_in<double>* ptr_ker_translate = (axpy_backend_in<double>*) backend_data;
  CoCoPeLiaSelectDevice(ptr_ker_translate->dev_id);

#ifdef DEBUG
  fprintf(stderr,"cublas_wrap_daxpy: cublasDaxpy(dev_id = %d,\
    N = %d, alpha = %lf, x = %p, incx = %d, y = %p, incy = %d)\n",
    ptr_ker_translate->dev_id, ptr_ker_translate->N, ptr_ker_translate->alpha,
    (double*) *ptr_ker_translate->x, ptr_ker_translate->incx,
    (double*) *ptr_ker_translate->y, ptr_ker_translate->incy);
#endif
  if(((CQueue_p)queue_wrap_p)->cqueue_backend_ctx[((CQueue_p)queue_wrap_p)->backend_ctr]) 
    cuCtxSetCurrent(*((CUcontext*)((CQueue_p)queue_wrap_p)->cqueue_backend_ctx[((CQueue_p)queue_wrap_p)->backend_ctr]));
  cublasHandle_t temp_handle = *((cublasHandle_t*)((CQueue_p)queue_wrap_p)->cqueue_backend_data
    [((CQueue_p)queue_wrap_p)->backend_ctr]);

  massert(CUBLAS_STATUS_SUCCESS == cublasDaxpy(temp_handle,
    ptr_ker_translate->N, (double*) &ptr_ker_translate->alpha, (double*) *ptr_ker_translate->x,
    ptr_ker_translate->incx, (double*) *ptr_ker_translate->y, ptr_ker_translate->incy),
    "cublas_wrap_daxpy failed\n");
}

void cublas_wrap_daxpby(void* backend_data, void* queue_wrap_p){
  axpby_backend_in<double>* ptr_ker_translate = (axpby_backend_in<double>*) backend_data;
  CoCoPeLiaSelectDevice(ptr_ker_translate->dev_id);

#ifdef DEBUG
  fprintf(stderr,"cublas_wrap_daxpby:\n cublasDscal(dev_id = %d,\
    N = %d, alpha = %lf, x = %p, incx = %d)\ncublasDaxpy(dev_id = %d,\
    N = %d, alpha = %lf, x = %p, incx = %d, y = %p, incy = %d)\n",
    ptr_ker_translate->dev_id, ptr_ker_translate->N, ptr_ker_translate->beta, (double*) *ptr_ker_translate->y, ptr_ker_translate->incy,
    ptr_ker_translate->dev_id, ptr_ker_translate->N, ptr_ker_translate->alpha, 
    (double*) *ptr_ker_translate->x, ptr_ker_translate->incx,
    (double*) *ptr_ker_translate->y, ptr_ker_translate->incy);
#endif
  if(((CQueue_p)queue_wrap_p)->cqueue_backend_ctx[((CQueue_p)queue_wrap_p)->backend_ctr]) 
    cuCtxSetCurrent(*((CUcontext*)((CQueue_p)queue_wrap_p)->cqueue_backend_ctx[((CQueue_p)queue_wrap_p)->backend_ctr]));
  cublasHandle_t temp_handle = *((cublasHandle_t*)((CQueue_p)queue_wrap_p)->cqueue_backend_data
    [((CQueue_p)queue_wrap_p)->backend_ctr]);

  massert(CUBLAS_STATUS_SUCCESS == cublasDscal(temp_handle,
    ptr_ker_translate->N, (double*) &ptr_ker_translate->beta, (double*) *ptr_ker_translate->y,
    ptr_ker_translate->incy), "cublasDscal failed\n");
  massert(CUBLAS_STATUS_SUCCESS == cublasDaxpy(temp_handle,
    ptr_ker_translate->N, (double*) &ptr_ker_translate->alpha, (double*) *ptr_ker_translate->x,
    ptr_ker_translate->incx, (double*) *ptr_ker_translate->y, ptr_ker_translate->incy),
    "cublasDaxpy failed\n");
}

void cublas_wrap_ddot(void* backend_data, void* queue_wrap_p){
  dot_backend_in<double>* ptr_ker_translate = (dot_backend_in<double>*) backend_data;
  CoCoPeLiaSelectDevice(ptr_ker_translate->dev_id);
  cublasHandle_t temp_handle = *((cublasHandle_t*)((CQueue_p)queue_wrap_p)->cqueue_backend_data
    [((CQueue_p)queue_wrap_p)->backend_ctr]);

  massert(CUBLAS_STATUS_SUCCESS == cublasDdot(temp_handle, ptr_ker_translate->N,
      (double*) *ptr_ker_translate->x, ptr_ker_translate->incx, (double*) *ptr_ker_translate->y,
      ptr_ker_translate->incy, (double*)ptr_ker_translate->result),
    "cublas_wrap_ddot failed\n");
}

void cublas_wrap_dgemm(void* backend_data, void* queue_wrap_p){
  gemm_backend_in<double>* ptr_ker_translate = (gemm_backend_in<double>*) backend_data;
#ifdef DEBUG
  int cur_dev_id = CoCoPeLiaGetDevice();
  if (ptr_ker_translate->dev_id != cur_dev_id)
    warning("cublas_wrap_dgemm: Changing device %d -> %d\n", cur_dev_id, ptr_ker_translate->dev_id);
#endif
  CoCoPeLiaSelectDevice(ptr_ker_translate->dev_id);
  
#ifdef DEBUG
  fprintf(stderr,"cublas_wrap_dgemm: cublasDgemm(dev_id = %d, TransA = %c, TransB = %c,\
    M = %d, N = %d, K = %d, alpha = %lf, A = %p, lda = %d, \n\
    B = %p, ldb = %d, beta = %lf, C = %p, ldC = %d)\n",
    ptr_ker_translate->dev_id, ptr_ker_translate->TransA, ptr_ker_translate->TransB,
    ptr_ker_translate->M, ptr_ker_translate->N, ptr_ker_translate->K, ptr_ker_translate->alpha,
    (double*) *ptr_ker_translate->A, ptr_ker_translate->ldA,
    (double*) *ptr_ker_translate->B, ptr_ker_translate->ldB,
    ptr_ker_translate->beta, (double*) *ptr_ker_translate->C, ptr_ker_translate->ldC);
#endif
  if(((CQueue_p)queue_wrap_p)->cqueue_backend_ctx[((CQueue_p)queue_wrap_p)->backend_ctr]) 
    cuCtxSetCurrent(*((CUcontext*)((CQueue_p)queue_wrap_p)->cqueue_backend_ctx[((CQueue_p)queue_wrap_p)->backend_ctr]));
  cublasHandle_t temp_handle = *((cublasHandle_t*)((CQueue_p)queue_wrap_p)->cqueue_backend_data
    [((CQueue_p)queue_wrap_p)->backend_ctr]);

  massert(CUBLAS_STATUS_SUCCESS == cublasDgemm(temp_handle,
    OpCharToCublas(ptr_ker_translate->TransA), OpCharToCublas(ptr_ker_translate->TransB),
    ptr_ker_translate->M, ptr_ker_translate->N, ptr_ker_translate->K, &ptr_ker_translate->alpha,
    (double*) *ptr_ker_translate->A, ptr_ker_translate->ldA,
    (double*) *ptr_ker_translate->B, ptr_ker_translate->ldB,
    &ptr_ker_translate->beta, (double*) *ptr_ker_translate->C, ptr_ker_translate->ldC),
    "cublas_wrap_dgemm: cublasDgemm failed\n");
}

void cublas_wrap_sgemm(void* backend_data, void* queue_wrap_p){
  gemm_backend_in<float>* ptr_ker_translate = (gemm_backend_in<float>*) backend_data;
#ifdef DDEBUG
  int cur_dev_id = CoCoPeLiaGetDevice();
  if (ptr_ker_translate->dev_id != cur_dev_id)
    warning("cublas_wrap_sgemm: Changing device %d -> %d\n", cur_dev_id, ptr_ker_translate->dev_id);
#endif
  CoCoPeLiaSelectDevice(ptr_ker_translate->dev_id);
#ifdef DDEBUG
  fprintf(stderr,"cublas_wrap_sgemm: cublasDgemm(dev_id = %d, TransA = %c, TransB = %c,\
    M = %d, N = %d, K = %d, alpha = %lf, A = %p, lda = %d, \n\
    B = %p, ldb = %d, beta = %lf, C = %p, ldC = %d)\n",
    ptr_ker_translate->dev_id, ptr_ker_translate->TransA, ptr_ker_translate->TransB,
    ptr_ker_translate->M, ptr_ker_translate->N, ptr_ker_translate->K, ptr_ker_translate->alpha,
    (float*) *ptr_ker_translate->A, ptr_ker_translate->ldA,
    (float*) *ptr_ker_translate->B, ptr_ker_translate->ldB,
    ptr_ker_translate->beta, (float*) *ptr_ker_translate->C, ptr_ker_translate->ldC);
#endif

  cublasHandle_t temp_handle = *((cublasHandle_t*)((CQueue_p)queue_wrap_p)->cqueue_backend_data
    [((CQueue_p)queue_wrap_p)->backend_ctr]);

  massert(CUBLAS_STATUS_SUCCESS == cublasSgemm(temp_handle,
    OpCharToCublas(ptr_ker_translate->TransA), OpCharToCublas(ptr_ker_translate->TransB),
    ptr_ker_translate->M, ptr_ker_translate->N, ptr_ker_translate->K, &ptr_ker_translate->alpha,
    (float*) *ptr_ker_translate->A, ptr_ker_translate->ldA,
    (float*) *ptr_ker_translate->B, ptr_ker_translate->ldB,
    &ptr_ker_translate->beta, (float*) *ptr_ker_translate->C, ptr_ker_translate->ldC),
    "cublas_wrap_dgemm: cublasDgemm failed\n");
}

void cublas_wrap_dgemv(void* backend_data, void* queue_wrap_p){
  gemv_backend_in<double>* ptr_ker_translate = (gemv_backend_in<double>*) backend_data;
#ifdef DDEBUG
  int cur_dev_id = CoCoPeLiaGetDevice();
  if (ptr_ker_translate->dev_id != cur_dev_id)
    warning("cublas_wrap_dgemv: Changing device %d -> %d\n", cur_dev_id, ptr_ker_translate->dev_id);
#endif
#ifdef DDEBUG
  fprintf(stderr,"cublas_wrap_dgemv: cblas_dgemv(dev_id = %d, TransA = %c\
    M = %d, N = %d,alpha = %lf, A = %p, lda = %d, \n\
    beta = %lf, x = %p, incx = %d, y = %p, incy = %d)\n",
    ptr_ker_translate->dev_id, ptr_ker_translate->TransA,
    ptr_ker_translate->M, ptr_ker_translate->N, ptr_ker_translate->alpha,
    (double*) *ptr_ker_translate->A, ptr_ker_translate->ldA,
    (double*) *ptr_ker_translate->x, ptr_ker_translate->incx,
    ptr_ker_translate->beta, (double*) *ptr_ker_translate->y, ptr_ker_translate->incy);
#endif

  cublasHandle_t temp_handle = *((cublasHandle_t*)((CQueue_p)queue_wrap_p)->cqueue_backend_data
    [((CQueue_p)queue_wrap_p)->backend_ctr]);

  massert(CUBLAS_STATUS_SUCCESS == cublasDgemv(temp_handle, OpCharToCublas(ptr_ker_translate->TransA),
    ptr_ker_translate->M, ptr_ker_translate->N, &ptr_ker_translate->alpha,
    (double*) *ptr_ker_translate->A, ptr_ker_translate->ldA,
    (double*) *ptr_ker_translate->x, ptr_ker_translate->incx,
    &ptr_ker_translate->beta, (double*) *ptr_ker_translate->y, ptr_ker_translate->incy),
    "cublas_wrap_dgemv: cublasDgemv failed\n");
}
