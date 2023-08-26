///
/// \author Anastasiadis Petros (panastas@cslab.ece.ntua.gr)
///
/// \brief "DataTile" class function implementations.
///

#include "DataTile.hpp"
#include "backend_wrappers.hpp"

int transfer_link_sharing[LOC_NUM][LOC_NUM][2];
CQueue_p recv_queues[LOC_NUM][LOC_NUM] = {{NULL}}, wb_queues[LOC_NUM][LOC_NUM] = {{NULL}}, exec_queue[LOC_NUM] = {NULL};

int DataTile::get_dtype_size() {
    if (dtype == DOUBLE) return sizeof(double);
    else if (dtype == FLOAT) return sizeof(float);
    else error("dtypesize: Unknown type");
}

void DataTile::set_loc_idx(int loc_idx, int val){
    loc_map[loc_idx] = val; 
}

long DataTile::get_chunk_size(int loc_idx){
    error("Must never be called for parent DataTile class\n");
    return -42;
}

short DataTile::get_initial_location() 
{ 
    //TODO: not implemented for multiple initial tile locations
    for (int iloc = 0; iloc < LOC_NUM; iloc++) if (!loc_map[iloc]) return deidxize(iloc); 
    warning("DataTile::get_initial_location: No initial location found");
    return -42; 
}

WR_properties DataTile::get_WRP(){
    return WRP;
}

void DataTile::set_WRP(WR_properties inprop){
    this->WRP = inprop;
}

const char* DataTile::get_WRP_string(){
    switch (WRP)
    {
    case RONLY: 
      return "RONLY";
    case WONLY:  
      return "WONLY";
    case WR:  
      return "WR";
    case WREDUCE:  
      return "WREDUCE";
    default:
        error("DataTile::get_WRP_string(): Unknown WRP\n");
    }
    return "UNREACHABLE";
}

int DataTile::size() { return get_dtype_size()*dim1*dim2;}

void DataTile::fetch(int priority_loc_id)
{
  if (!(WRP == WR || WRP== RONLY)) error("DataTile::fetch called with WRP = %s\n", get_WRP_string());
  
  set_loc_idx(idxize(priority_loc_id), 2);

#ifdef DEBUG
	fprintf(stderr, "|-----> DataTile[%d:%d,%d]::fetch(%d) : loc_map = %s\n", 
    id, GridId1, GridId2, priority_loc_id, printlist(loc_map, LOC_NUM));
#endif

  LinkRoute_p best_route = new LinkRoute();
  best_route->starting_hop = 0;

  best_route->optimize(this); // The core of our optimization

#ifdef DEBUG
  fprintf(stderr, "DataTile[%d:%d,%d]::fetch(%d) WRP = %s, Road = %s \n", id, GridId1, GridId2, 
    priority_loc_id, get_WRP_string(), printlist(best_route->hop_uid_list, best_route->hop_num));
#endif

	CBlock_p block_ptr[best_route->hop_num] = {NULL};
  block_ptr[0] = StoreBlock[idxize(best_route->hop_uid_list[0])]; 
  block_ptr[0]->add_reader();
  best_route->hop_buf_list[0] = block_ptr[0]->Adrs;
  best_route->hop_ldim_list[0] = get_chunk_size(idxize(best_route->hop_uid_list[0]));

	for(int inter_hop = 1 ; inter_hop < best_route->hop_num; inter_hop++){
    best_route->hop_ldim_list[inter_hop] = get_chunk_size(idxize(best_route->hop_uid_list[inter_hop]));  // TODO: This might be wrong for Tile1D + inc!=1

    if (best_route->hop_uid_list[inter_hop] != priority_loc_id){ // We do not have a block assigned already in this case
      if(WRP == RONLY){
        //if(tmp->StoreBlock[idxize(best_route->hop_uid_list[1+inter_hop])] != NULL) // TODO: Is thi needed?
        //	tmp->StoreBlock[idxize(best_route->hop_uid_list[1+inter_hop])]->Owner_p = NULL;
        
          block_ptr[inter_hop] = StoreBlock[idxize(best_route->hop_uid_list[inter_hop])] = 
            Global_Buffer_2D[idxize(best_route->hop_uid_list[inter_hop])]->assign_Cblock(SHARABLE,false);
          block_ptr[inter_hop]->set_owner((void**)&StoreBlock[idxize(best_route->hop_uid_list[inter_hop])],false);
        }
        else block_ptr[inter_hop] = Global_Buffer_2D[idxize(best_route->hop_uid_list[inter_hop])]->assign_Cblock(EXCLUSIVE,false);
    }
    else block_ptr[inter_hop] = StoreBlock[idxize(priority_loc_id)];

    best_route->hop_buf_list[inter_hop] = block_ptr[inter_hop]->Adrs;
    best_route->hop_event_list[inter_hop-1] = block_ptr[inter_hop]->Available;
    best_route->hop_cqueue_list[inter_hop-1] = recv_queues[idxize(best_route->hop_uid_list[inter_hop])][idxize(best_route->hop_uid_list[inter_hop-1])];

  }
  best_route->hop_cqueue_list[0]->wait_for_event(block_ptr[0]->Available); // TODO: is this needed for all optimization methods?
  
  FasTCoCoMemcpy2DAsync(best_route, dim1, dim2, get_dtype_size());
  
  CQueue_p used_queue = best_route->hop_cqueue_list[best_route->hop_num-2];

  if(WRP == WR){
      for(int inter_hop = 1 ; inter_hop < best_route->hop_num - 1; inter_hop++){
        CBlock_wrap_p wrap_inval = NULL;
        wrap_inval = (CBlock_wrap_p) malloc (sizeof(struct CBlock_wrap));
        wrap_inval->lockfree = false;
        wrap_inval->CBlock = block_ptr[inter_hop];
        best_route->hop_cqueue_list[inter_hop-1]->add_host_func((void*)&CBlock_RW_INV_wrap, (void*) wrap_inval);
      }
      loc_map[idxize(best_route->hop_uid_list[best_route->hop_num-1])] = 42;

  }
  else{
    for(int inter_hop = 1 ; inter_hop < best_route->hop_num; inter_hop++)
      loc_map[idxize(best_route->hop_uid_list[inter_hop])] = 42; 

  }
  if (WRP == WR){
    CBlock_wrap_p wrap_inval = NULL;
    wrap_inval = (CBlock_wrap_p) malloc (sizeof(struct CBlock_wrap));
    wrap_inval->lockfree = false;
    wrap_inval->CBlock = StoreBlock[idxize(best_route->hop_uid_list[0])];
    used_queue->add_host_func((void*)&CBlock_RR_INV_wrap, (void*) wrap_inval);
  }
  else{
    CBlock_wrap_p wrap_read = (CBlock_wrap_p) malloc (sizeof(struct CBlock_wrap));
    wrap_read->CBlock = StoreBlock[idxize(best_route->hop_uid_list[0])];
    wrap_read->lockfree = false;
    used_queue->add_host_func((void*)&CBlock_RR_wrap, (void*) wrap_read);
  }
#ifdef DEBUG
	fprintf(stderr, "<-----|\n");
#endif
}

void DataTile::writeback(){
	short W_master_idx = idxize(W_master);
	short Writeback_id = get_initial_location(), Writeback_id_idx = idxize(Writeback_id);
	if (WRP != WR) return;
	if (StoreBlock[W_master_idx] == NULL || StoreBlock[W_master_idx]->State == INVALID)
			error("DataTile::writeback -> Tile(%d.[%d,%d]) Storeblock[%d] is NULL\n",
				id, GridId1, GridId2, W_master_idx);
	if (StoreBlock[Writeback_id_idx] == NULL)
			error("DataTile::writeback -> Tile(%d.[%d,%d]) Storeblock[%d] is NULL\n",
				id, GridId1, GridId2, Writeback_id_idx);
	if (W_master_idx == Writeback_id);
	else{
    LinkRoute_p best_route = new LinkRoute();
    best_route->starting_hop = 0;
    best_route->optimize_reverse(this); // The core of our optimization

  massert(W_master_idx == idxize(best_route->hop_uid_list[0]), 
    "DataTile::writeback error -> W_master_idx [%d] != idxize(best_route->hop_uid_list[0]) [%d]", 
    W_master_idx, idxize(best_route->hop_uid_list[0]));

#ifdef DEBUG
	fprintf(stderr, "|-----> DataTile[%d:%d,%d]::writeback() : loc_map = %s\n", 
    id, GridId1, GridId2, printlist(loc_map, LOC_NUM));
#endif

    best_route->hop_ldim_list[0] = get_chunk_size(W_master_idx);
    best_route->hop_buf_list[0] = StoreBlock[W_master_idx]->Adrs;
    CBlock_p block_ptr[best_route->hop_num] = {NULL};
    block_ptr[0] = StoreBlock[W_master_idx]; 

    for(int inter_hop = 1 ; inter_hop < best_route->hop_num; inter_hop++){
      best_route->hop_ldim_list[inter_hop] = get_chunk_size(idxize(best_route->hop_uid_list[inter_hop]));  // TODO: This might be wrong for Tile1D + inc!=1

      if(inter_hop < best_route->hop_num - 1) block_ptr[inter_hop] = Global_Buffer_2D[idxize(best_route->hop_uid_list[inter_hop])]->assign_Cblock(EXCLUSIVE,false);
      else block_ptr[inter_hop] = StoreBlock[Writeback_id_idx];
  
      best_route->hop_buf_list[inter_hop] = block_ptr[inter_hop]->Adrs;

      best_route->hop_event_list[inter_hop-1] = block_ptr[inter_hop]->Available;
      best_route->hop_cqueue_list[inter_hop-1] = wb_queues[idxize(best_route->hop_uid_list[inter_hop])][idxize(best_route->hop_uid_list[inter_hop-1])];

    }
    best_route->hop_cqueue_list[0]->wait_for_event(W_complete);
    CQueue_p used_queue = best_route->hop_cqueue_list[best_route->hop_num-2];

  #ifdef DEBUG
    fprintf(stderr, "DataTile::writeback WRP = %s, Road = %s \n", get_WRP_string() ,
      printlist(best_route->hop_uid_list, best_route->hop_num));
  #endif
    FasTCoCoMemcpy2DAsync(best_route, dim1, dim2, get_dtype_size());

    for(int inter_hop = 1 ; inter_hop < best_route->hop_num -1; inter_hop++){
      CBlock_wrap_p wrap_inval = NULL;
      wrap_inval = (CBlock_wrap_p) malloc (sizeof(struct CBlock_wrap));
      wrap_inval->lockfree = false;
      wrap_inval->CBlock = block_ptr[inter_hop];
      best_route->hop_cqueue_list[inter_hop-1]->add_host_func((void*)&CBlock_RW_INV_wrap, (void*) wrap_inval);
    }
  }
#ifndef ASYNC_ENABLE
	CoCoSyncCheckErr();
#endif
}