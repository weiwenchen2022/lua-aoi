#include "aoi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct alloc_cookie {
    int count;
    size_t max;
    size_t current;
} alloc_cookie;

static void *my_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    alloc_cookie *cookie = (alloc_cookie *)ud;

    if (nsize == 0) {
	cookie->count--;
	cookie->current -= osize;
	free(ptr);
	return NULL;
    }

    if (ptr == NULL)
	cookie->count++;

    cookie->current -= osize;
    cookie->current += nsize;
    if (cookie->max < cookie->current)
	cookie->max = cookie->current;

    return realloc(ptr, nsize);
}

typedef struct OBJECT {
    float pos[3];
    float v[3];
    char mode[4];
} OBJECT;

static OBJECT OBJ[4];

static inline void init_obj(uint32_t id, float x, float y , float vx, float vy, const char *mode)
{
    OBJ[id].pos[0] = x;
    OBJ[id].pos[1] = y;
    OBJ[id].pos[2] = 0;

    OBJ[id].v[0] = vx;
    OBJ[id].v[1] = vy;
    OBJ[id].v[2] = 0;

    strncpy(OBJ[id].mode, mode, sizeof(OBJ[id].mode));
    OBJ[id].mode[sizeof(OBJ[id].mode) - 1] = '\0';
}

static void update_obj(aoi_space *space, uint32_t id)
{
    int i;

    for (i = 0; i < 3; i++) {
	OBJ[id].pos[i] += OBJ[id].v[i];
	if (OBJ[id].pos[i] < 0)
	    OBJ[id].pos[i] += 100.0f;
	else if (OBJ[id].pos[i] > 100.0f)
	    OBJ[id].pos[i] -= 100.0f;
    }

    aoi_update(space, id, OBJ[id].mode, OBJ[id].pos);
}

static void message(void *ud, uint32_t watcher, uint32_t marker)
{
    (void)ud;

    fprintf(stderr, "%u (%.0f, %.0f) => %u (%.0f, %.0f)\n",
	    watcher, OBJ[watcher].pos[0], OBJ[watcher].pos[1],
	    marker, OBJ[marker].pos[0], OBJ[marker].pos[1]
    );
}

static void test(aoi_space *space)
{
    init_obj(0, 40, 0, 0, 1, "w");
    init_obj(1, 42, 100, 0, -1, "wm");
    init_obj(2, 0, 40, 1, 0, "w");
    init_obj(3, 100, 45, -1, 0, "wm");

    int i, j;
    for (i = 0; i < 100; i++) {
	if (i < 50) {
	    for (j = 0; j < 4; j++)
		update_obj(space, j);
	} else if (i == 50) {
	    aoi_update(space, 3, "d", OBJ[3].pos);
	} else {
	    for (j = 0; j < 3; j++)
		update_obj(space, j);
	}

	aoi_message(space, message, NULL);
    }
}

int main(void)
{
    alloc_cookie cookie = {0, 0, 0,};
    aoi_space *space = aoi_create(10.0f, my_alloc, &cookie);

    test(space);

    aoi_release(space);

    fprintf(stderr, "max memory = %zd, current memory = %zd\n", cookie.max, cookie.current);

    return 0;
}
