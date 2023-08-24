
#include <iostream>

#include "linkmap.hpp"
#include "DataTile.hpp"

#define P2P_FETCH_FROM_INIT

// Naive fetch from initial data loc every time
// Similar to cuBLASXt
#ifdef P2P_FETCH_FROM_INIT 
void LinkRoute::optimize(void* transfer_tile_wrapped){
#ifdef DEBUG
	fprintf(stderr, "|-----> LinkRoute::optimize()\n");
#endif
    DataTile_p transfer_tile = (DataTile_p) transfer_tile_wrapped;
    hop_num = 2;
    int start_hop = -42, end_hop = -42;
    for(int ctr = 0; ctr < LOC_NUM; ctr++)
    {
        if(transfer_tile->loc_map[ctr] == 0) start_hop = deidxize(ctr);
        if(transfer_tile->loc_map[ctr] == 2) end_hop = deidxize(ctr);
    }
	hop_uid_list[0] = start_hop;
    hop_uid_list[1] = end_hop;
#ifdef DEBUG
	fprintf(stderr, "<-----|\n");
#endif
}

void LinkRoute::optimize_reverse(void* transfer_tile_wrapped){
    DataTile_p transfer_tile = (DataTile_p) transfer_tile_wrapped;
    hop_num = 2;
    int start_hop = -42, end_hop = -42;
    for(int ctr = 0; ctr < LOC_NUM; ctr++)
    {
        if(transfer_tile->loc_map[ctr] == 42) start_hop = deidxize(ctr);
        if(transfer_tile->loc_map[ctr] == 0) end_hop = deidxize(ctr);
    }
	hop_uid_list[0] = start_hop;
    hop_uid_list[1] = end_hop;

}
#endif

// Outdated fetch with preference to GPU tiles but with simple serial search for src
// Similar to BLASX behaviour when its assumed topology does not fit to the interconnect
#ifdef P2P_FETCH_FROM_GPU_SERIAL 
void LinkRoute::optimize(void* transfer_tile_wrapped){
    DataTile_p transfer_tile = (DataTile_p) transfer_tile_wrapped;
    hop_num = 2;
    int start_hop = -42, end_hop = -42;
    for(int ctr = 0; ctr < LOC_NUM; ctr++)
    {
        if(transfer_tile->loc_map[ctr] == 0 || transfer_tile->loc_map[ctr] == 42) start_hop = deidxize(ctr);
        if(transfer_tile->loc_map[ctr] == 2) end_hop = deidxize(ctr);
    }
	hop_uid_list[0] = start_hop;
    hop_uid_list[1] = end_hop;

}

void LinkRoute::optimize_reverse(void* transfer_tile_wrapped){
    DataTile_p transfer_tile = (DataTile_p) transfer_tile_wrapped;
    hop_num = 2;
    int start_hop = -42, end_hop = -42;
    for(int ctr = 0; ctr < LOC_NUM; ctr++)
    {
        if(transfer_tile->loc_map[ctr] == 42) start_hop = deidxize(ctr);
        if(transfer_tile->loc_map[ctr] == 0) end_hop = deidxize(ctr);
    }
	hop_uid_list[0] = start_hop;
    hop_uid_list[1] = end_hop;

}
#endif

// Fetch selection based on 'distance' from available sources (if multiple exist)
// Similar to XKBLAS
#ifdef P2P_FETCH_FROM_GPU_DISTANCE
void LinkRoute::optimize(void* transfer_tile_wrapped){
    DataTile_p transfer_tile = (DataTile_p) transfer_tile_wrapped;
    hop_num = 2;
    int start_hop = -42, end_hop = -42;
    for(int ctr = 0; ctr < LOC_NUM; ctr++) if(transfer_tile->loc_map[ctr] == 2) end_hop = deidxize(ctr);
    hop_uid_list[1] = end_hop;

    int end_hop_idx = idxize(end_hop);    
    int pos_max = LOC_NUM;
    double link_bw_max = 0;
    for (int pos =0; pos < LOC_NUM; pos++) if (transfer_tile->loc_map[pos] == 0 || transfer_tile->loc_map[pos] == 42){
        double current_link_bw = final_estimated_link_bw[end_hop_idx][pos];
        if (current_link_bw > link_bw_max){
          link_bw_max = current_link_bw;
          pos_max = pos;
        }
    }
    hop_uid_list[0] = deidxize(pos_max);
}
#endif

// Fetch selection based on 'distance' from available sources (if multiple exist) 
// Exactly like PARALiA 1.5 
#ifdef P2P_FETCH_FROM_GPU_DISTANCE_PLUS
#endif