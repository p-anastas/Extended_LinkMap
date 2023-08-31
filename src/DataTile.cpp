///
/// \author Anastasiadis Petros (panastas@cslab.ece.ntua.gr)
///
/// \brief "DataTile" class function implementations.
///

#include "DataTile.hpp"
#include "backend_wrappers.hpp"

int links_share_bandwidth[LOC_NUM][LOC_NUM][2];
CQueue_p recv_queues[LOC_NUM][LOC_NUM] = {{NULL}};
CQueue_p wb_queues[LOC_NUM][LOC_NUM] = {{NULL}};
CQueue_p exec_queue[LOC_NUM][MAX_BACKEND_L] = {{NULL}};
int exec_queue_ctr[LOC_NUM] = {-1}; 

int DataTile::get_dtype_size() {
    if (dtype == DOUBLE) return sizeof(double);
    else if (dtype == FLOAT) return sizeof(float);
    else error("dtypesize: Unknown type");
    return -1;
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
    case WR_LAZY:  
      return "WR_LAZY";   
    case W_REDUCE:  
      return "W_REDUCE";
    default:
        error("DataTile::get_WRP_string(): Unknown WRP\n");
    }
    return "UNREACHABLE";
}

int DataTile::size() { return get_dtype_size()*dim1*dim2;}

void DataTile::fetch(CBlock_p target_block, int priority_loc_id)
{
  if (!(WRP == WR || WRP== RONLY || WRP == WR_LAZY)) error("DataTile::fetch called with WRP = %s\n", get_WRP_string());
  
  set_loc_idx(idxize(priority_loc_id), 2);

#ifdef DEBUG
	fprintf(stderr, "|-----> DataTile[%d:%d,%d]::fetch(%d) : loc_map = %s\n", 
    id, GridId1, GridId2, priority_loc_id, printlist(loc_map, LOC_NUM));
#endif

  LinkRoute_p best_route = new LinkRoute();
  best_route->starting_hop = 0;

  best_route->optimize(this, 1); // The core of our optimization

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
    else block_ptr[inter_hop] = target_block;

    best_route->hop_buf_list[inter_hop] = block_ptr[inter_hop]->Adrs;
    best_route->hop_event_list[inter_hop-1] = block_ptr[inter_hop]->Available;
    best_route->hop_cqueue_list[inter_hop-1] = 
      recv_queues[idxize(best_route->hop_uid_list[inter_hop])]
      [idxize(best_route->hop_uid_list[inter_hop-1])];
    ETA_set(best_route->hop_cqueue_list[inter_hop-1]->ETA_get(), best_route->hop_uid_list[inter_hop]);

  }
  best_route->hop_cqueue_list[0]->wait_for_event(block_ptr[0]->Available); // TODO: is this needed for all optimization methods?
  
  FasTCoCoMemcpy2DAsync(best_route, dim1, dim2, get_dtype_size());
  
  CQueue_p used_queue = best_route->hop_cqueue_list[best_route->hop_num-2];

  if(WRP == WR || WRP == W_REDUCE || WRP == WR_LAZY){
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
  if (WRP == WR || WRP == W_REDUCE || WRP == WR_LAZY){
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

void DataTile::operations_complete(CQueue_p assigned_exec_queue){
  short W_master_idx = idxize(W_master);
  if(WR == WRP){
    W_complete->record_to_queue(assigned_exec_queue);
#ifdef SUBKERNELS_FIRE_WHEN_READY
    writeback();
#endif
  }
  else if(WR_LAZY == WRP){
    CBlock_p temp_block = Global_Buffer_2D[W_master_idx]->assign_Cblock(EXCLUSIVE,false);
    temp_block->set_owner(NULL,false);
    fetch(temp_block, W_master);
    assigned_exec_queue->wait_for_event(temp_block->Available);
    axpy_backend_in<double>* backend_axpy_wrapper = (axpy_backend_in<double>*) malloc(sizeof(struct axpy_backend_in<double>));
    backend_axpy_wrapper->N = dim1*dim2;
    backend_axpy_wrapper->incx = backend_axpy_wrapper->incy = 1;
    backend_axpy_wrapper->alpha = reduce_mult;
    backend_axpy_wrapper->dev_id = W_master;
    backend_axpy_wrapper->x = (void**) &(temp_block->Adrs);
    backend_axpy_wrapper->y = (void**) &(StoreBlock[W_master_idx]->Adrs);
    backend_run_operation(backend_axpy_wrapper, "Daxpy", assigned_exec_queue);
    W_complete->record_to_queue(assigned_exec_queue);
#ifdef SUBKERNELS_FIRE_WHEN_READY
    writeback();
#endif
    CBlock_wrap_p wrap_inval = NULL;
    wrap_inval = (CBlock_wrap_p) malloc (sizeof(struct CBlock_wrap));
    wrap_inval->lockfree = false;
    wrap_inval->CBlock = temp_block;
    assigned_exec_queue->add_host_func((void*)&CBlock_RW_INV_wrap, (void*) wrap_inval);
  }
}

void DataTile::writeback(){
	short W_master_idx = idxize(W_master);
	short Writeback_id = get_initial_location(), Writeback_id_idx = idxize(Writeback_id);
	if (!(WRP == WR || WRP == WR_LAZY))
    error("DataTile::writeback -> Tile(%d.[%d,%d]) has WRP = %s\n",
			id, GridId1, GridId2, WRP);
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

      if(inter_hop < best_route->hop_num - 1){
        block_ptr[inter_hop] = Global_Buffer_2D[idxize(best_route->hop_uid_list[inter_hop])]->
          assign_Cblock(EXCLUSIVE,false);
          best_route->hop_event_list[inter_hop-1] = block_ptr[inter_hop]->Available;
      }
      else{
        block_ptr[inter_hop] = StoreBlock[Writeback_id_idx];
        best_route->hop_event_list[inter_hop-1] = NULL;
      }
  
      best_route->hop_buf_list[inter_hop] = block_ptr[inter_hop]->Adrs;

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

/*****************************************************/
/// PARALia 2.0 - timed queues and blocks

void DataTile::ETA_add_task(long double task_duration, int dev_id){
	block_ETA[idxize(dev_id)] += task_duration;
}

void DataTile::ETA_set(long double new_workload_t, int dev_id){
	block_ETA[idxize(dev_id)] = new_workload_t; 
}

long double DataTile::ETA_get(int dev_id){
	return block_ETA[idxize(dev_id)];
}

long double DataTile::ETA_fetch_estimate(int target_id){

  long double result = 1e9; 
  int temp_val = loc_map[idxize(target_id)];
  set_loc_idx(idxize(target_id), 2);

  LinkRoute_p best_route = new LinkRoute();
  best_route->starting_hop = 0;

  result = best_route->optimize(this, 0);

  set_loc_idx(idxize(target_id), temp_val);

  return result; 
}
