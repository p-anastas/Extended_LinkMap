///
/// \author Anastasiadis Petros (panastas@cslab.ece.ntua.gr)
///
/// \brief The "Asset" related function implementations.
///

#include "Decomposer.hpp"
#include "linkmap.hpp"

Tile1D* Decom1D::getTile(int iloc){
  if(iloc >= GridSz) error("Decom1D::getTile : iloc >= GridSz (%d vs %d)\n", iloc, GridSz);
  return Tile_map[iloc];
}

void Decom1D::SyncTileMap(){
   for (int itt = 0; itt < GridSz; itt++)
      Tile_map[itt]->writeback();
}

void Decom1D::DestroyTileMap(){
  int current_ctr;
  for (int itt = 0 ; itt < GridSz; itt++){
    current_ctr = itt;
    delete Tile_map[current_ctr];
  }
  free(Tile_map);
}

void Decom1D::DrawTileMap(){
  fprintf(stderr, " Tile1D representation: \
                 \n ______________________ \
                 \n|      id[GridId]      |\
                 \n| - - - - - - - - - - -|\
                 \n|        (dim1)        |\
                 \n| - - - - - - - - - - -|\
                 \n| - - WR_properties - -|\
                 \n| - - - - - - - - - - -|\
                 \n| - - - loc_list  - - -|\
                 \n|______________________|\n\n");

  for (int itt = 0 ; itt < GridSz; itt++)
    fprintf(stderr, " ______________________ ");
  fprintf(stderr, "\n");
  for (int itt = 0 ; itt < GridSz; itt++)
    fprintf(stderr, "|     %4d[%6d]     |",
    Tile_map[itt]->id,
    Tile_map[itt]->GridId1);
  fprintf(stderr, "\n");
  for (int itt = 0 ; itt < GridSz; itt++)
    fprintf(stderr, "| - - - - - - - - - - -|");
  fprintf(stderr, "\n");
  for (int itt = 0 ; itt < GridSz; itt++)
    fprintf(stderr, "|       (%6d)       |",
    Tile_map[itt]->dim1);
  fprintf(stderr, "\n");
  for (int itt = 0 ; itt < GridSz; itt++)
    fprintf(stderr, "| - - - - - - - - - - -|");
  fprintf(stderr, "\n");
  for (int itt = 0 ; itt < GridSz; itt++)
    fprintf(stderr, "%s", Tile_map[itt]->get_WRP_string());
  fprintf(stderr, "\n");
  for(int loctr = 0; loctr < LOC_NUM; loctr++){
    for (int itt = 0 ; itt < GridSz; itt++)
      fprintf(stderr, "| - - - - - - - - - - -|");
    fprintf(stderr, "\n");
    for (int itt = 0 ; itt < GridSz; itt++)
      fprintf(stderr, "%s", printlist<int>(Tile_map[itt]->loc_map, LOC_NUM));
    fprintf(stderr, "\n");
  }
  for (int itt = 0 ; itt < GridSz; itt++)
   fprintf(stderr, "|______________________|");
  fprintf(stderr, "\n\n");
}
