#include "aoi.c"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define MAX 1000

static aoi_space *SPACE = NULL;
static int INDEX[MAX];

static void shuffle(int number)
{
    int i;

    for (i = 0; i < number; i++) {
	int idx1 = i + rand() % (number - i);
	int idx2 = i + rand() % (number - i);
	int temp = INDEX[idx1];
	INDEX[idx1] = INDEX[idx2];
	INDEX[idx2] = temp;
    }
}

static void init(int number, int start)
{
    int i;

    for (i = 0; i < number; i++)
	INDEX[i] = (i + start) * 8;
}

static inline void mnew(map *m, uint32_t id)
{
    object *obj = map_query(SPACE, m, id);
    assert(id == obj->id);
    assert(obj->ref == 1);
}

static inline void mdelete(map *m, uint32_t id)
{
    object *obj = map_drop(m, id);
    assert(obj != NULL);
    assert(id == obj->id);
    assert(obj->ref == 1);
    drop_object(SPACE, obj);
}

static void testmap(map *m, int n, int start)
{
    int i;

    init(n, start);
    shuffle(n);

    for (i = 0; i < n; i++)
	mnew(m, INDEX[i]);

    shuffle(n);
    n = rand() % (n/2);
    for (i = 0; i < n; i++)
	mdelete(m, INDEX[i]);
}

static void checkmap(void *ud, object *obj)
{
    map *m = (map *)ud;
    fprintf(stderr, "%u ", obj->id);
    mdelete(m, obj->id);
}

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

int main(void)
{
    alloc_cookie cookie = {0, 0, 0,};
    SPACE = aoi_create(10.0f, my_alloc, &cookie);

    map *m = map_new(SPACE);

    int i;
    for (i = 0; i < 10; i++)
	testmap(m, 10, i*10);

    testmap(m, 100, 100);
    testmap(m, 200, 200);
    testmap(m, 500, 500);

    map_foreach(m, checkmap, m);

    map_delete(SPACE, m);

    aoi_release(SPACE);

    fprintf(stderr, "\nmax memory = %zd, current memory = %zd\n", cookie.max, cookie.current);

    return 0;
}
