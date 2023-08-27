///
/// \author Anastasiadis Petros (panastas@cslab.ece.ntua.gr)
///
/// \brief Generalised DataTiles for 1D and 2D tile definition, transfer and sharing. 
///

#ifndef DATATILE_H
#define DATATILE_H

#include "DataCaching.hpp"

enum dtype_enum{
		DOUBLE,
		FLOAT
};

enum WR_properties{
		RONLY = 1,
		WONLY = 2,
        WR = 3,
        WREDUCE = 4
};

typedef class DataTile{
public:
    dtype_enum dtype;
    int id, GridId1, GridId2;
    int dim1, dim2;
    WR_properties WRP;
    int W_master = -42;
    int W_pending = -42;
    Event_p W_complete = NULL; 
    int W_master_backend_ctr = -42;

    // loc_map values mean: 
    // - not available = -42
    // - available in location = 42 (exhept initial)
    // - initial location = 0
    // - priority target loc = 2, 
    // - other target loc(s) = 1
    int loc_map[LOC_NUM]; 
    CBlock_p StoreBlock[LOC_NUM];
    double block_ETA[LOC_NUM] = {-42}; 

    // General Functions
    int get_dtype_size();
    short get_initial_location();
    WR_properties get_WRP();
    const char* get_WRP_string();
    int size();
    virtual long get_chunk_size(int loc_idx);

    void set_loc_idx(int loc_idx, int val);
    void set_WRP(WR_properties inprop);

    void fetch(int priority_loc_id);
    void writeback();
}* DataTile_p;

class Tile1D : public DataTile {
public:
    int inc[LOC_NUM];

    // Constructor
    Tile1D(void* tile_addr, int T1tmp,
        int inc, int inGrid1, dtype_enum dtype_in, CBlock_p init_loc_block_p);
    //Destructor
    ~Tile1D();

    long get_chunk_size(int loc_idx); 
};

class Tile2D : public DataTile {
public:    
    int ldim[LOC_NUM];
	// Constructor
	Tile2D(void* tile_addr, int T1tmp, int T2tmp,
			int ldim, int inGrid1, int inGrid2, dtype_enum dtype_in, CBlock_p init_loc_block_p);
	//Destructor
	~Tile2D();

    long get_chunk_size(int loc_idx); 
};

#endif