/*
 * gql_tz.c
 *    Lookups in the generated IANA time zone table (gql_tzdata.c).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime/gql_tz.h"

const gql_tz_zone *gql_tz_find(const char *name)
{
    if (!name) return NULL;
    for (int i = 0; i < gql_tz_zone_count; i++) {
        if (strcmp(gql_tz_zones[i].name, name) == 0) return &gql_tz_zones[i];
    }
    return NULL;
}

int32_t gql_tz_offset_at_utc(const gql_tz_zone *zone, int64_t utc)
{
    if (!zone) return 0;
    /* last transition at or before utc */
    int lo = 0, hi = zone->count - 1, found = -1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (zone->transitions[mid].utc <= utc) { found = mid; lo = mid + 1; }
        else hi = mid - 1;
    }
    return found < 0 ? zone->initial_offset : zone->transitions[found].offset;
}

int32_t gql_tz_offset_at_local(const gql_tz_zone *zone, int64_t local)
{
    if (!zone) return 0;
    /* An offset o is valid for the local time when local - o falls in the
     * span where o applies. Try the offset in effect a day earlier first
     * (the earlier offset in an overlap), then the one a day later. */
    int32_t before = gql_tz_offset_at_utc(zone, local - 86400 - 14 * 3600);
    if (gql_tz_offset_at_utc(zone, local - before) == before) return before;
    int32_t after = gql_tz_offset_at_utc(zone, local + 86400);
    if (gql_tz_offset_at_utc(zone, local - after) == after) return after;
    /* In a gap: the offset before it. */
    return before;
}

void gql_tz_format_offset(int32_t offset, char *buf, int buflen)
{
    char sign = offset < 0 ? '-' : '+';
    int32_t a = offset < 0 ? -offset : offset;
    int h = a / 3600, m = (a % 3600) / 60, s = a % 60;
    if (s) snprintf(buf, (size_t)buflen, "%c%02d:%02d:%02d", sign, h, m, s);
    else   snprintf(buf, (size_t)buflen, "%c%02d:%02d", sign, h, m);
}
