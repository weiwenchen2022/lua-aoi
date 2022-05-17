#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct aoi_space aoi_space;

typedef void *(*aoi_Alloc)(void *ud, void *ptr, size_t osize, size_t nsize);
aoi_space *aoi_create(float radius, aoi_Alloc alloc, void *ud);

aoi_space *aoi_new(float radius);

void aoi_release(aoi_space *space);

/* w(atcher) m(arker) d(rop) */
void aoi_update(aoi_space *space, uint32_t id, const char *mode, const float pos[3]);

typedef void (*aoi_Callback)(void *ud, uint32_t watcher, uint32_t marker);
void aoi_message(aoi_space *space, aoi_Callback cb, void *ud);
