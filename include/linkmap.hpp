///
/// \author Anastasiadis Petros (panastas@cslab.ece.ntua.gr)
///
/// \brief The headers for functions for general use throught CoCoPeLia
///

#ifndef UNIHELPERS_H
#define UNIHELPERS_H

#include<iostream>
#include <string>

#include <cstdlib>
#include <cstring>

#include <stdio.h>
#include <stdarg.h>

#include <atomic>

/*****************************************************/
/// Generalised "Command queue" and "Event" definition (e.g. CUDA streams and Events for CUDA backend)

typedef class Event* Event_p;

typedef class CommandQueue
{
	private:
	public:
		void* cqueue_backend_ptr[MAX_BACKEND_L];
		void* cqueue_backend_data[MAX_BACKEND_L];
		void* cqueue_backend_ctx[MAX_BACKEND_L];
		int backend_ctr = 0;
		int simultaneous_workers = 1; 

		int dev_id;

		//Constructor
		CommandQueue(int dev_id, int mode);

		//Destructor
		~CommandQueue();
		void sync_barrier();
		void add_host_func(void* func, void* data);
		void wait_for_event(Event_p Wevent);
		int request_parallel_backend();
		void set_parallel_backend(int backend_ctr);

		std::string name;
		void print() { std::cout << "Command Queue : " << name; }

		/*****************************************************/
		/// PARALia 2.0 - simple timed queues (without slowdowns)
		// An estimation of when the queue will be free of tasks.
		long double queue_ETA = 0; 
		void ETA_add_task(long double task_fire_t, long double task_duration);
		void ETA_set(long double new_ETA);
		long double ETA_get();

}* CQueue_p;

enum event_status{
	UNRECORDED = 0, /// Event has not been recorded yet.
	RECORDED = 1, /// Recorded but not guaranteed to be complete.
	COMPLETE = 2,  /// Complete but not yet ran 'check' function (for cache updates etc)
	CHECKED = 3,  /// Complete and Checked/Updated caches etc.
	GHOST = 4  /// Does not exist in the time continuum.
};

/// Returns a string representation for event_status
const char* print_event_status(event_status in_status);

class Event
{
	private:
		event_status status;
	public:
		void* event_backend_ptr;
		int id, dev_id;

		/// Constructors
		Event(int dev_id);
		/// Destructors
		~Event();
		/// Functions
		void sync_barrier();
		void record_to_queue(CQueue_p Rr);
		event_status query_status();
		void checked();
		void reset();
		void soft_reset();

};

/*****************************************************/
/// Generalised data management functions & helpers

// Return current device used
int CoCoPeLiaGetDevice();

// Select device for current running pthread
void CoCoPeLiaSelectDevice(short dev_id);

// Return free memory and total memory for current device
void CoCoPeLiaDevGetMemInfo(long long* free_dev_mem, long long* max_dev_mem);

// Sync all devices and search for enchountered errors.
void CoCoSyncCheckErr();

// Search for enchountered errors without synchronization.
void CoCoASyncCheckErr();

// Enable available links for target device with all other devices
void CoCoEnableLinks(short target_dev_i, short num_devices);

// Malloc in loc with error-checking
void* CoCoMalloc(long long N_bytes, short loc);

// Free in loc with error-checking
void CoCoFree(void * ptr, short loc);

// Memcpy between two locations with errorchecking
void CoCoMemcpy(void* dest, void* src, long long N_bytes, short loc_dest, short loc_src);

// Memcpy between two locations with errorchecking
void CoCoMemcpy2D(void* dest, long int ldest, void* src, long int lsrc, long int rows, long int cols, short elemSize, short loc_dest, short loc_src);

// Asunchronous Memcpy between two locations WITHOUT synchronous errorchecking. Use with caution.
void CoCoMemcpyAsync(void* dest, void* src, long long N_bytes, short loc_dest, short loc_src, CQueue_p transfer_medium);

// Asunchronous Memcpy between two locations WITHOUT synchronous errorchecking.
void CoCoMemcpy2DAsync(void* dest, long int ldest, void* src, long int lsrc, long int rows, long int cols,
	short elemSize, short loc_dest, short loc_src, CQueue_p transfer_medium);
// Asunchronous Memcpy between two locations WITHOUT synchronous errorchecking and with TTEST logging disabled (for avoiding double logs for hop tranfers)
void CoCoMemcpy2DAsync_noTTs(void* dest, long int ldest, void* src, long int lsrc, long int rows, long int cols,
	short elemSize, short loc_dest, short loc_src, CQueue_p transfer_medium);

// Print and log bandwidths and links used with CoCoMemcpy2DAsync. Unusable with TTEST flag
void n_HopMemcpyPrint();

// Initalize vector in loc with error-checking
template<typename VALUETYPE>
extern void CoCoVecInit(VALUETYPE *vec, long long length, int seed, short loc);
// Helper for Parallel OpenMP vector initialization
template<typename VALUETYPE>
extern void CoCoParallelVecInitHost(VALUETYPE *vec, long long length, int seed);

// Return the max dim size (which is a multiple of 'step') for 'Asset2DNum' square assets on 'loc'
long int CoCoGetMaxDimSqAsset2D(short Asset2DNum, short dsize, long int step, short loc);

// Return the max dim size (which is a multiple of 'step') for 'Asset1DNum' assets on 'loc'
long int CoCoGetMaxDimAsset1D(short Asset1DNum, short dsize, long int step, short loc);

short CoCoGetPtrLoc(const void * in_ptr);


/*****************************************************/
/// LinkRoute stuff

// Struct for multi-hop optimized transfers
typedef class LinkRoute{
public:
	int hop_num;
	int hop_uid_list[LOC_NUM];
	void* hop_buf_list[LOC_NUM];
	int hop_ldim_list[LOC_NUM];
	int starting_hop = 0;

	CQueue_p hop_cqueue_list[LOC_NUM-1];
	Event_p hop_event_list[LOC_NUM-1];
	
	// Target: 0/42 -> 2 [+1] 
	long double optimize(void* transfer_tile_wrapped, int update_ETA_flag);
	// Target: 42 -> 0 
	long double optimize_reverse(void* transfer_tile_wrapped, int update_ETA_flag); 
	// Target: SRC -> ? -> dest 
	long double optimize_hop_route(void* transfer_tile_wrapped, int update_ETA_flag, int dest_loc, int src_loc); 
}* LinkRoute_p;

// A memcpy implementation using multiple units as intermendiate hops for a better transfer bandwidth
void FasTCoCoMemcpy2DAsync(LinkRoute_p roadMap, long int rows, long int cols, short elemSize);
// Print and log bandwidths and links used with FasTCoCoMemcpy2DAsync. Unusable with TTEST flag
void HopMemcpyPrint();

/*****************************************************/
/// Timers for benchmarks
// CPU accurate timer
double csecond();

// Event timer for background Event timing (~usually ms accuracy)
typedef class Event_timer
{
	private:
		Event_p Event_start;
		Event_p Event_stop;
		double time_ms;
	public:
		int dev_id;

		Event_timer(int dev_id);
		void start_point(CQueue_p start_queue);
		void stop_point(CQueue_p stop_queue);
		double sync_get_time();
}* Event_timer_p;

/*****************************************************/
/// Print functions
#if !defined(PRINTFLIKE)
#if defined(__GNUC__)
#define PRINTFLIKE(n,m) __attribute__((format(printf,n,m)))
#else
#define PRINTFLIKE(n,m) /* If only */
#endif /* __GNUC__ */
#endif /* PRINTFLIKE */

template<typename VALUETYPE>
extern const char *printlist(VALUETYPE *list, int length);

void lprintf(short lvl, const char *fmt, ...)PRINTFLIKE(2,3);
void massert(bool condi, const char *fmt, ...)PRINTFLIKE(2,3);
void error(const char *fmt, ...)PRINTFLIKE(1,2);
void warning(const char *fmt, ...)PRINTFLIKE(1,2);

/*****************************************************/
/// Enum(and internal symbol) functions
// Memory layout struct for matrices
enum mem_layout { ROW_MAJOR = 0, COL_MAJOR = 1 };
const char *print_mem(mem_layout mem);

// Print name of loc for transfers
//const char *print_loc(short loc); FIXME: Remove

/*****************************************************/
/// General benchmark functions

inline double Drandom() { return (double)rand() / (double)RAND_MAX;}
short Dtest_equality(double* C_comp, double* C, long long size);
short Stest_equality(float* C_comp, float* C, long long size);

double Gval_per_s(long long value, double time);
long long gemm_flops(long int M, long int N, long int K);
long long gemm_memory(long int M, long int N, long int K, long int A_loc, long int B_loc, long int C_loc, short dsize);

long long gemv_flops(long int M, long int N);
long long gemv_memory(long int M, long int N, long int A_loc, long int x_loc, long int y_loc, short dsize);

long long axpy_flops(long int  N);
long long axpy_memory(long int N, long int x_loc, long int y_loc, short dsize);

long long dot_flops(long int  N);
long long dot_memory(long int N, long int x_loc, long int y_loc, short dsize);

long int count_lines(FILE* fp); // TODO: Where is this used?
void check_benchmark(char *filename);

/*****************************************************/
int gcd (int a, int b, int c);
inline short idxize(short num){ return (num == -1) ? LOC_NUM - 1: num;}
inline short deidxize(short idx){ return (idx == LOC_NUM - 1) ? -1 : idx;}
inline short remote(short loc, short other_loc){ return (loc == other_loc) ? 0 : 1;}
inline int is_in_list(int elem, int* elem_list, int list_len){ for (int idx = 0; idx < list_len; idx++)
		if(elem_list[idx] == elem) return 1; return 0; }
void translate_binary_to_unit_list(int case_id, int* active_unit_num_p, int* active_unit_id_list);

/// double/float arethmetic comparrison that compares based on NORMALIZE_NEAR_SPLIT_LIMIT minimum difference.
int normal_equal(double n1, double n2);
int normal_less(double n1, double n2);
int normal_larger(double n1, double n2);
int normal_lessequal(double n1, double n2);
int normal_largerequal(double n1, double n2);

/*****************************************************/
/// LinkMap stuff

#define MAX_ALLOWED_HOPS 1
#define MAX_HOP_ROUTES 1
#define HOP_PENALTY 0.2

typedef class Modeler* MD_p; 

typedef class LinkMap{
	public:
		// Empirically obtained values for links if used independently
		double link_lat[LOC_NUM][LOC_NUM] = {{0}};
		double link_bw[LOC_NUM][LOC_NUM] = {{0}};

		// Estimated bandwidth values for links if used silmuntaniously for a given problem
		double link_bw_shared[LOC_NUM][LOC_NUM] = {{0}};
		double link_bw_shared_hops[LOC_NUM][LOC_NUM] = {{0}};

		// Number of current link uses. TODO: For runtime optimization, not implemented
		int link_active[LOC_NUM][LOC_NUM] = {{0}};
		long long link_uses[LOC_NUM][LOC_NUM] = {{0}};

		// TODO: These are not currently utilized, but they are 'free' to obtain so...
		int link_hop_num[LOC_NUM][LOC_NUM];
		int link_hop_route_num[LOC_NUM][LOC_NUM];
		int link_hop_route[LOC_NUM][LOC_NUM][MAX_ALLOWED_HOPS][LOC_NUM]; 
/********************** Initialization/Modification ***************************/
		LinkMap();
		~LinkMap();
/******************************************************************************/
/**************************** Helper Fuctions *********************************/
		void print_link_active();
		void print_link_bw();
		void print_link_bw_shared();
		void print_link_bw_shared_hops();
		void copy(class LinkMap* other_linkmap);
		void reset(); // Resets all but the initial link_lat and link_bw to zero.
		void reset_links(int unit_id); // Resets all link(s) related with unit_id (the initial link and all passing through it)

/******************************************************************************/
/************************ Class main Functions ********************************/
		void update_link_weights(MD_p* list_of_models, int T);
		void update_link_shared_weights(MD_p* list_of_models,
			int* active_unit_id_list, int active_unit_num);
		void update_link_hop_shared_weights(MD_p* list_of_models,
			int* active_unit_id_list, int active_unit_num);
		double linkmap_shared_bw_unroll(int dest, int src);
}* LinkMap_p;

extern int links_share_bandwidth[LOC_NUM][LOC_NUM][2];
extern int links_share_bandwidth_init;
double shared_bw_unroll(int dest, int src);

extern CQueue_p recv_queues[LOC_NUM][LOC_NUM];
extern CQueue_p wb_queues[LOC_NUM][LOC_NUM];
extern CQueue_p exec_queue[LOC_NUM][MAX_BACKEND_L];
extern int exec_queue_ctr[LOC_NUM]; 
extern double final_estimated_link_bw[LOC_NUM][LOC_NUM];
extern int final_link_active[LOC_NUM][LOC_NUM];

extern LinkMap_p final_estimated_linkmap;

#include <execinfo.h>
#include <signal.h>
#include <unistd.h>

void handler(int sig);

#endif