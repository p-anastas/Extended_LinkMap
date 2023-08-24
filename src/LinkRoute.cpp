
#include <iostream>

#include "linkmap.hpp"
#include "DataTile.hpp"

LinkMap_p final_estimated_linkmap = NULL;
double final_estimated_link_bw[LOC_NUM][LOC_NUM];

#define P2P_FETCH_FROM_GPU_DISTANCE_PLUS

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
        if(start_hop!= -42 && end_hop != -42) break;
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
void LinkRoute::optimize(void* transfer_tile_wrapped){
    DataTile_p transfer_tile = (DataTile_p) transfer_tile_wrapped;
    hop_num = 2;
    int start_hop = -42, end_hop = -42;
    for(int ctr = 0; ctr < LOC_NUM; ctr++) if(transfer_tile->loc_map[ctr] == 2) end_hop = deidxize(ctr);
    hop_uid_list[1] = end_hop;

    int end_hop_idx = idxize(end_hop);    
    int pos_max = LOC_NUM;
    double link_bw_max = 0;
    for (int pos =0; pos < LOC_NUM; pos++){
        if (pos == end_hop_idx || transfer_tile->StoreBlock[pos] == NULL) {
      //if (StoreBlock[pos]!= NULL)
      //  error("Tile2D(%d)::getClosestReadLoc(%d): Should not be called, Tile already available in %d.\n",  id, end_hop, end_hop);
      continue;
    }
    //StoreBlock[pos]->update_state(false);
    state temp = transfer_tile->StoreBlock[pos]->State;
    if (temp == AVAILABLE || temp == SHARABLE || temp == NATIVE){
      event_status block_status = transfer_tile->StoreBlock[pos]->Available->query_status();
#ifdef ALLOW_FETCH_RECORDED
      if(block_status == COMPLETE || block_status == CHECKED || block_status == RECORDED){
#else
      if(block_status == COMPLETE || block_status == CHECKED){
#endif
        double current_link_bw = final_estimated_link_bw[end_hop_idx][pos];
        if (block_status == RECORDED) current_link_bw-=current_link_bw*FETCH_UNAVAILABLE_PENALTY;
        if (current_link_bw > link_bw_max){
          link_bw_max = current_link_bw;
          pos_max = pos;
        }
        else if (current_link_bw == link_bw_max &&
        final_estimated_linkmap->link_uses[end_hop_idx][pos] < final_estimated_linkmap->link_uses[end_hop_idx][pos_max]){
          link_bw_max = current_link_bw;
          pos_max = pos;
        }
      }
    }
  }
#ifdef DEBUG
  fprintf(stderr, "|-----> LinkRoute::optimize(DataTile[%d:%d,%d]): Selecting src = %d for dest = %d\n", transfer_tile->id, transfer_tile->dim1, transfer_tile->dim2, pos_max, end_hop);
#endif
  if (pos_max >= LOC_NUM) error("LinkRoute::optimize(DataTile[%d:%d,%d]): No location found for tile - bug\n", transfer_tile->id, transfer_tile->dim1, transfer_tile->dim2);
  //Global_Cache[pos_max]->lock();
  CBlock_p temp_outblock = transfer_tile->StoreBlock[pos_max];
  if(temp_outblock != NULL){
    temp_outblock->lock();
    //temp_outblock->update_state(true);
    state temp = temp_outblock->State;
    event_status block_status = temp_outblock->Available->query_status();
    if ((temp == AVAILABLE || temp == SHARABLE || temp == NATIVE) &&
#ifdef ALLOW_FETCH_RECORDED
    (block_status == COMPLETE || block_status == CHECKED|| block_status == RECORDED)){
#else
    (block_status == COMPLETE || block_status == CHECKED)){
#endif
      temp_outblock->add_reader(true);
      //Global_Cache[pos_max]->unlock();
      temp_outblock->unlock();
      #ifdef DDEBUG
        lprintf(lvl-1, "<-----|\n");
      #endif
      final_estimated_linkmap->link_uses[end_hop_idx][pos_max]++;
      hop_uid_list[0] = deidxize(pos_max);
      return; 
    }
    else error("Tile1D(%d)::getClosestReadLoc(%d): pos_max = %d selected,\
      but something changed after locking its cache...fixme\n", transfer_tile->id, end_hop, pos_max);
  }
  else error("Tile1D(%d)::getClosestReadLoc(%d): pos_max = %d selected,\
    but StoreBlock[pos_max] was NULL after locking its cache...fixme\n", transfer_tile->id, end_hop, pos_max);
  return;
}
#endif

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