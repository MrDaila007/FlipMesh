#include "fm_position.h"
#include <stdio.h>

void position_format_coords(int32_t lat_i, int32_t lon_i, char* buf, size_t len) {
    /* lat_i and lon_i are degrees × 1e7 */
    int32_t lat_abs = lat_i < 0 ? -lat_i : lat_i;
    int32_t lon_abs = lon_i < 0 ? -lon_i : lon_i;

    uint32_t lat_deg  = (uint32_t)(lat_abs / 10000000);
    uint32_t lat_frac = (uint32_t)((lat_abs % 10000000) / 1000); /* 4 decimal places */
    uint32_t lon_deg  = (uint32_t)(lon_abs / 10000000);
    uint32_t lon_frac = (uint32_t)((lon_abs % 10000000) / 1000);

    char ns = (lat_i >= 0) ? 'N' : 'S';
    char ew = (lon_i >= 0) ? 'E' : 'W';

    snprintf(buf, len, "%lu.%04lu%c %lu.%04lu%c",
             (unsigned long)lat_deg, (unsigned long)lat_frac, ns,
             (unsigned long)lon_deg, (unsigned long)lon_frac, ew);
}

uint32_t position_calc_distance_m(int32_t lat1, int32_t lon1,
                                   int32_t lat2, int32_t lon2) {
    /* Equirectangular approximation with integer arithmetic.
       1 degree latitude ≈ 111 319 m.
       Longitude correction: cos(lat) ≈ 1 - lat²/2 for small angles is
       inaccurate for large latitudes; use a fixed-point cos table is overkill.
       Simple approach: treat longitude degrees as 111 319 × cos(avg_lat).
       For embedded use, we approximate cos(lat) with a lookup at 5° steps. */
    static const uint32_t cos_table[19] = {
        /* cos(0°)..cos(90°) at 5° steps, scaled × 1000 */
        1000, 996, 985, 966, 940, 906, 866, 819,
        766,  707, 643, 574, 500, 423, 342, 259,
        174,  87,  0
    };

    int32_t dlat = lat2 - lat1; /* in 1e-7 degrees */
    int32_t dlon = lon2 - lon1;
    int32_t avg_lat_i = (lat1 + lat2) / 2;
    if(avg_lat_i < 0) avg_lat_i = -avg_lat_i;

    uint32_t avg_lat_deg = (uint32_t)(avg_lat_i / 10000000);
    if(avg_lat_deg > 90) avg_lat_deg = 90;
    uint32_t cos_val = cos_table[avg_lat_deg / 5]; /* rough 5° step */

    /* Convert 1e-7 degree units to meters:
       dlat_m = dlat × 111319 / 1e7 = dlat × 11132 / 1e6
       dlon_m = dlon × 111319 × cos / 1e7 / 1000 */
    int32_t dlat_m = (int32_t)((int64_t)dlat * 11132 / 1000000);
    int32_t dlon_m = (int32_t)((int64_t)dlon * 11132 / 1000000);
    dlon_m = (int32_t)((int64_t)dlon_m * cos_val / 1000);

    /* Integer sqrt via Newton's method */
    uint32_t d2 = (uint32_t)(dlat_m * dlat_m) + (uint32_t)(dlon_m * dlon_m);
    if(d2 == 0) return 0;

    uint32_t x = d2;
    uint32_t y = (x + 1) / 2;
    while(y < x) {
        x = y;
        y = (x + d2 / x) / 2;
    }
    return x;
}

void position_format_distance(uint32_t meters, char* buf, size_t len) {
    if(meters < 1000) {
        snprintf(buf, len, "%lum", (unsigned long)meters);
    } else {
        uint32_t km_int  = meters / 1000;
        uint32_t km_frac = (meters % 1000) / 100;
        snprintf(buf, len, "%lu.%lukm", (unsigned long)km_int, (unsigned long)km_frac);
    }
}
