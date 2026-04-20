#pragma once
#include <stdint.h>
#include <stddef.h>

/* Format lat/lon (×1e-7 degrees) as "55.7520°N 37.6175°E" */
void position_format_coords(int32_t lat_i, int32_t lon_i, char* buf, size_t len);

/* Calculate approximate distance in meters between two lat/lon points.
   Uses an integer equirectangular approximation — accurate to ~5% for
   distances under 100 km, no trig, safe for embedded use. */
uint32_t position_calc_distance_m(int32_t lat1, int32_t lon1,
                                   int32_t lat2, int32_t lon2);

/* Format a distance in meters as "123m" or "1.2km" */
void position_format_distance(uint32_t meters, char* buf, size_t len);
