#ifndef DGN_DELVE_H
#define DGN_DELVE_H

class map_lines;

void delve(map_lines *map, int ngb_min = 2, int ngb_max = 3,
                           int connchance = 0, int cellnum = -1, int top = 125);

#endif
