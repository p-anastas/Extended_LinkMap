///
/// \author Anastasiadis Petros (panastas@cslab.ece.ntua.gr)
///
/// \brief "Tile2D" class function implementations.
///

#include "linkmap.hpp"
#include "DataTile.hpp"

int Tile2D_num = 0;

Tile2D::Tile2D(void *in_addr, int in_dim1, int in_dim2,
               int in_ldim, int inGrid1, int inGrid2, dtype_enum dtype_in, CBlock_p init_loc_block_p)
{
  short lvl = 3;
#ifdef DDEBUG
  lprintf(lvl - 1, "|-----> Tile2D(%d)::Tile2D(in_addr(%d),%d,%d,%d, %d, %d)\n",
          Tile2D_num, CoCoGetPtrLoc(in_addr), in_dim1, in_dim2, in_ldim, inGrid1, inGrid2);
#endif
  dtype = dtype_in;
  dim1 = in_dim1;
  dim2 = in_dim2;
  GridId1 = inGrid1;
  GridId2 = inGrid2;
  id = Tile2D_num;
  Tile2D_num++;
  short init_loc = CoCoGetPtrLoc(in_addr);
  short init_loc_idx = idxize(init_loc);
  for (int iloc = 0; iloc < LOC_NUM; iloc++)
  {
    if (iloc == init_loc_idx)
    {
      loc_map[iloc] = 0;
      StoreBlock[iloc] = init_loc_block_p;
      StoreBlock[iloc]->Adrs = in_addr;
      StoreBlock[iloc]->set_owner((void **)&StoreBlock[iloc], false);
      ldim[iloc] = in_ldim;
      StoreBlock[iloc]->Available->record_to_queue(NULL);
    }
    else
    {
      StoreBlock[iloc] = NULL;
      ldim[iloc] = in_dim1;
      loc_map[iloc] = -42;
    }
  }
#ifdef DDEBUG
  lprintf(lvl - 1, "<-----|\n");
#endif
}

Tile2D::~Tile2D()
{
  short lvl = 3;
  Tile2D_num--;
}

long Tile2D::get_chunk_size(int loc_idx)
{
  return ldim[loc_idx];
}