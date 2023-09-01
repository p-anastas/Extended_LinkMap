///
/// \author Anastasiadis Petros (panastas@cslab.ece.ntua.gr)
///
/// \brief The "Asset" related function implementations.
///

#include"linkmap.hpp"

typedef class ProblemMetadata* PMD_p;
PMD_p PMD_cache[PROBLEM_MD_CACHE] = {NULL}; 
int PMD_cache_entries = 0;

int links_share_bandwidth[LOC_NUM][LOC_NUM][2];
LinkMap_p final_estimated_linkmap = NULL;
double final_estimated_link_bw[LOC_NUM][LOC_NUM];

CQueue_p recv_queues[LOC_NUM][LOC_NUM] = {{NULL}};
CQueue_p wb_queues[LOC_NUM][LOC_NUM] = {{NULL}};

CQueue_p exec_queue[LOC_NUM][MAX_BACKEND_L] = {{NULL}};
int exec_queue_ctr[LOC_NUM] = {-1}; 



