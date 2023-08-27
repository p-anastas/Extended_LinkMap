
#include <iostream>

#include "linkmap.hpp"
#include "DataTile.hpp"

LinkMap_p final_estimated_linkmap = NULL;
double final_estimated_link_bw[LOC_NUM][LOC_NUM];

#define CHAIN_FETCH_QUEUE_WORKLOAD

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
    if (temp == AVAILABLE || temp == SHARABLE || temp == NATIVE ){
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
/*
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
    */
   hop_uid_list[0] = deidxize(pos_max);
  return;
}
#endif

// Naive, for comparison reasons mainly
#ifdef CHAIN_FETCH_SERIAL
void LinkRoute::optimize(void* transfer_tile_wrapped){
    DataTile_p transfer_tile = (DataTile_p) transfer_tile_wrapped;
    hop_num = 1;
    for(int ctr = 0; ctr < LOC_NUM; ctr++){
      if(transfer_tile->loc_map[ctr] == 0) hop_uid_list[0] = deidxize(ctr);
      else if(transfer_tile->loc_map[ctr] == 1 || transfer_tile->loc_map[ctr] == 2)
        hop_uid_list[hop_num++] = deidxize(ctr);
    }
}
#endif

#ifdef CHAIN_FETCH_RANDOM
void LinkRoute::optimize(void* transfer_tile_wrapped){
    DataTile_p transfer_tile = (DataTile_p) transfer_tile_wrapped;
    hop_num = 0;
    int loc_list[LOC_NUM];
    for(int ctr = 0; ctr < LOC_NUM; ctr++){
      if(transfer_tile->loc_map[ctr] == 0) hop_uid_list[0] = deidxize(ctr);
      else if(transfer_tile->loc_map[ctr] == 1 || transfer_tile->loc_map[ctr] == 2)
        loc_list[hop_num++] = deidxize(ctr);
    } 
    int start_idx = int(rand() % hop_num); 
    int hop_ctr = 1;
    for(int ctr = start_idx; ctr < hop_num; ctr++) hop_uid_list[hop_ctr++] = loc_list[ctr];
    for(int ctr = 0; ctr < start_idx; ctr++) hop_uid_list[hop_ctr++] = loc_list[ctr];
    hop_num++;
}
#endif

#include <algorithm>
#include <iostream>
#include <list>
long factorial(const int n)
{
    long f = 1;
    for (int i=1; i<=n; ++i)
        f *= i;
    return f;
}

#ifdef CHAIN_FETCH_TIME
void LinkRoute::optimize(void* transfer_tile_wrapped){
    DataTile_p transfer_tile = (DataTile_p) transfer_tile_wrapped;
    hop_num = 0;
    std::list<int> loc_list;
    int tmp_hop; 
    double fire_est = csecond();
    for(int ctr = 0; ctr < LOC_NUM; ctr++){
      if(transfer_tile->loc_map[ctr] == 0) hop_uid_list[0] = deidxize(ctr);
      else if(transfer_tile->loc_map[ctr] == 1 || transfer_tile->loc_map[ctr] == 2){
        tmp_hop = deidxize(ctr);
        loc_list.push_back(tmp_hop);
        hop_num++;
      }
    }
    if (hop_num == 1) hop_uid_list[1] = tmp_hop;
    else{
      double best_t = 1e9;
      int best_list[factorial(hop_num)][hop_num]; 
      int flag = 1, tie_list_num = 0;
      while (flag){
        double max_t = -1, total_t = 0, temp_t;
        int temp_ctr = 0, prev = idxize(hop_uid_list[0]), templist[hop_num]; 
        for (int x : loc_list){
          templist[temp_ctr++] = x; 
          //fprintf(stderr,"Checking link[%d,%d] final_estimated_link_bw  = %lf\n", 
          // idxize(x), prev, final_estimated_link_bw[idxize(x)][prev]);
          temp_t = 1/final_estimated_link_bw[idxize(x)][prev]*transfer_tile->size()/1e9;
          if (temp_t > max_t) max_t = temp_t;
          total_t += temp_t;
          prev = idxize(x);
        }
        temp_t = max_t;// + (total_t - max_t)/STREAMING_BUFFER_OVERLAP;
        //fprintf(stderr,"Checking location list[%s]: temp_t = %lf\n", printlist(templist,hop_num), temp_t);
      
        //fprintf(stderr,"Checking location list[%s]: temp_ETA = %lf\n", printlist(templist,hop_num), temp_ETA);
        if(temp_t < best_t){
          best_t = temp_t;
          for (int ctr = 0; ctr < hop_num; ctr++) best_list[0][ctr] = templist[ctr];
          tie_list_num = 1;
        }
        else if (temp_t == best_t){
          for (int ctr = 0; ctr < hop_num; ctr++) best_list[tie_list_num][ctr] = templist[ctr];
          tie_list_num++;
        }
        flag = std::next_permutation(loc_list.begin(), loc_list.end());
      }
      //fprintf(stderr,"Selecting location list[%s]: min_ETA = %lf\n", printlist(best_list,hop_num), min_ETA);
      int rand_tie_list = int(rand() % tie_list_num); 
      for(int ctr = 0; ctr < hop_num; ctr++)
        hop_uid_list[ctr+1] = best_list[rand_tie_list][ctr];

    }
    hop_num++; 
}
#endif

#ifdef CHAIN_FETCH_QUEUE_WORKLOAD
void LinkRoute::optimize(void* transfer_tile_wrapped){
    DataTile_p transfer_tile = (DataTile_p) transfer_tile_wrapped;
    hop_num = 0;
    std::list<int> loc_list;
    int tmp_hop; 
    for(int ctr = 0; ctr < LOC_NUM; ctr++){
      if(transfer_tile->loc_map[ctr] == 0) hop_uid_list[0] = deidxize(ctr);
      else if(transfer_tile->loc_map[ctr] == 1 || transfer_tile->loc_map[ctr] == 2){
        tmp_hop = deidxize(ctr);
        loc_list.push_back(tmp_hop);
        hop_num++;
      }
    }
    if (hop_num == 1){
      hop_uid_list[1] = tmp_hop;
      recv_queues[idxize(hop_uid_list[1])][idxize(hop_uid_list[0])]->
        WT_add_task(1/final_estimated_link_bw[idxize(hop_uid_list[1])][idxize(hop_uid_list[0])]*transfer_tile->size()/1e9);
    }
    else{
      double min_ETA = 1e9;
      int best_list[factorial(hop_num)][hop_num]; 
      int flag = 1, tie_list_num = 0;
      while (flag){
        double max_t = -1, total_t = 0, temp_t;
        int temp_ctr = 0, prev = idxize(hop_uid_list[0]), templist[hop_num]; 
        for (int x : loc_list){
          templist[temp_ctr++] = x; 
          //fprintf(stderr,"Checking link[%d,%d] final_estimated_link_bw  = %lf\n", 
          // idxize(x), prev, final_estimated_link_bw[idxize(x)][prev]);
          temp_t = 1/final_estimated_link_bw[idxize(x)][prev]*transfer_tile->size()/1e9;
          if (temp_t > max_t) max_t = temp_t;
          total_t += temp_t;
          prev = idxize(x);
        }
        temp_t = max_t;// + (total_t - max_t)/STREAMING_BUFFER_OVERLAP;
        //fprintf(stderr,"Checking location list[%s]: temp_t = %lf\n", printlist(templist,hop_num), temp_t);
        prev = idxize(hop_uid_list[0]);
        double temp_ETA = 0, queue_ETA; 
        for(int ctr = 0; ctr < hop_num; ctr++){
            queue_ETA = recv_queues[idxize(templist[ctr])][prev]->workload_t + temp_t;
            //fprintf(stderr,"queue_ETA [%d -> %d]= %lf (temp_t = %lf)\n", deidxize(prev), templist[ctr], queue_ETA);
            prev = idxize(templist[ctr]);
            if(temp_ETA < queue_ETA) temp_ETA = queue_ETA;
            //fprintf(stderr,"Checking location list[%s]: queue_ETA = %lf\n", printlist(templist,hop_num), queue_ETA);
        }
        //fprintf(stderr,"Checking location list[%s]: temp_ETA = %lf\n", printlist(templist,hop_num), temp_ETA);
        if(abs(temp_ETA - min_ETA)/temp_t > NORMALIZE_NEAR_SPLIT_LIMIT && temp_ETA < min_ETA){
          min_ETA = temp_ETA;
          for (int ctr = 0; ctr < hop_num; ctr++) best_list[0][ctr] = templist[ctr];
          tie_list_num = 1;
//#ifdef PDEBUG
        //fprintf(stderr,"DataTile[%d:%d,%d]: New min_ETA(%lf) for route = %s\n", 
        //  transfer_tile->id, transfer_tile->GridId1, transfer_tile->GridId2, min_ETA, 
        //  printlist(best_list[tie_list_num-1],hop_num));
//#endif
        }
        else if (abs(temp_ETA - min_ETA)/temp_t <= NORMALIZE_NEAR_SPLIT_LIMIT){
          for (int ctr = 0; ctr < hop_num; ctr++) best_list[tie_list_num][ctr] = templist[ctr];
          tie_list_num++;
//#ifdef PDEBUG
        //  fprintf(stderr,"DataTile[%d:%d,%d]: same min_ETA(%lf) for candidate(%d) route = %s\n", 
        //    transfer_tile->id, transfer_tile->GridId1, transfer_tile->GridId2, temp_ETA, 
        //    tie_list_num, printlist(best_list[tie_list_num-1],hop_num));
//#endif
        }
        flag = std::next_permutation(loc_list.begin(), loc_list.end());
      }
      
      int rand_tie_list = int(rand() % tie_list_num); 
//#ifdef PDEBUG
      //fprintf(stderr,"DataTile[%d:%d,%d]: Selected route = %s from %d candidates with ETA = %lf\n", 
      //    transfer_tile->id, transfer_tile->GridId1, transfer_tile->GridId2,
      //    printlist(best_list[tie_list_num-1],hop_num), tie_list_num, min_ETA);
//#endif
      for(int ctr = 0; ctr < hop_num; ctr++){
        hop_uid_list[ctr+1] = best_list[rand_tie_list][ctr];
        recv_queues[idxize(hop_uid_list[ctr+1])][idxize(hop_uid_list[ctr])]->
          WT_set(min_ETA);
      }
    }
    hop_num++; 
}
#endif

// TODO: hop writeback transfers not implemented
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

