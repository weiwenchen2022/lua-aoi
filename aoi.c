#include "aoi.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define AOI_RADIS 10.0f
#define AOI_RADIS2 (AOI_RADIS * AOI_RADIS)

#define INVALID_ID (~0)
#define PRE_ALLOC 16

#define DIST2(p1, p2) ( \
	(p1[0] - p2[0]) * (p1[0] - p2[0]) \
	+ (p1[1] - p2[1]) * (p1[1] - p2[1]) \
	+ (p1[2] - p2[2]) * (p1[2] - p2[2]) \
	)

#define MODE_WATCHER 1
#define MODE_MARKER 2
#define MODE_MOVE 4
#define MODE_DROP 8

typedef struct object {
    int ref;
    uint32_t id;
    int version;
    int mode;
    float last[3];
    float position[3];
} object;

typedef struct object_set {
    int cap;
    int number;
    object **slot;
} object_set;

typedef struct pair_list {
    struct pair_list *next;

    object *watcher;
    int watcher_version;

    object *marker;
    int marker_version;
} pair_list;

typedef struct map_slot {
    uint32_t id;
    object *obj;
    int next;
} map_slot;

typedef struct map {
    int size;
    int lastfree;
    map_slot *slot;
} map;

typedef struct aoi_space {
    float radius;
    float radius2;

    aoi_Alloc alloc;
    void *alloc_ud;

    map *object;
    object_set *watcher_static;
    object_set *marker_static;
    object_set *watcher_move;
    object_set *marker_move;
    pair_list *hot;
} aoi_space;

static inline object *new_object(aoi_space *space, uint32_t id)
{
    object *obj = space->alloc(space->alloc_ud, NULL, 0, sizeof(*obj));
    obj->ref = 1;
    obj->id = id;
    obj->version = 0;
    obj->mode = 0;

    return obj;
}

static inline map_slot *mainposition(map *m, uint32_t id)
{
    uint32_t hash = id & (m->size - 1);
    return &m->slot[hash];
}

static void rehash(aoi_space *space, map *m);

static void map_insert(aoi_space *space, map *m, uint32_t id, object *obj)
{
    map_slot *s = mainposition(m, id);
    if (INVALID_ID == s->id) {
	s->id = id;
	s->obj = obj;
	return;
    }

    if (s != mainposition(m, s->id)) {
	map_slot *last = mainposition(m, s->id);
	while (s - m->slot != last->next) {
	    assert(last->next >= 0);
	    last = &m->slot[last->next];
	}

	uint32_t temp_id = s->id;
	object *temp_obj = s->obj;

	last->next = s->next;
	s->id = id;
	s->obj = obj;
	s->next = -1;

	if (temp_obj)
	    map_insert(space, m, temp_id, temp_obj);

	return;
    }

    while (m->lastfree >= 0) {
	map_slot *temp = &m->slot[m->lastfree--];
	if (INVALID_ID == temp->id) {
	    temp->id = id;
	    temp->obj = obj;
	    temp->next = s->next;
	    s->next = (int)(temp - m->slot);

	    return;
	}
    }

    rehash(space, m);
    map_insert(space, m, id, obj);
}

static void rehash(aoi_space *space, map *m)
{
    map_slot *old_slot = m->slot;
    int old_size = m->size;
    m->size = 2 * old_size;
    m->lastfree = m->size - 1;
    m->slot = space->alloc(space->alloc_ud, NULL, 0, m->size * sizeof(map_slot));
    int i;

    for (i = 0; i < m->size; i++) {
	map_slot *s = &m->slot[i];
	s->id = INVALID_ID;
	s->obj = NULL;
	s->next = -1;
    }

    for (i = 0; i < old_size; i++) {
	map_slot *s = &old_slot[i];
	if (s->obj)
	    map_insert(space, m, s->id, s->obj);
    }

    space->alloc(space->alloc_ud, old_slot, old_size * sizeof(map_slot), 0);
}

static object *map_query(aoi_space *space, map *m, uint32_t id)
{
    map_slot *s = mainposition(m, id);

    for (;;) {
	if (id == s->id) {
	    if (s->obj == NULL)
		s->obj = new_object(space, id);

	    return s->obj;
	}

	if (s->next < 0)
	    break;

	s = &m->slot[s->next];
    }

    object *obj = new_object(space, id);
    map_insert(space, m, id, obj);
    return obj;
}

static void map_foreach(map *m, void (*func)(void *ud, object *obj), void *ud)
{
    int i;

    for (i = 0; i < m->size; i++)
	if (m->slot[i].obj)
	    func(ud, m->slot[i].obj);
}

static object *map_drop(map *m, uint32_t id)
{
    map_slot *s = mainposition(m, id);

    for (;;) {
	if (id == s->id) {
	    object *obj = s->obj;
	    s->obj = NULL;
	    return obj;
	}

	if (s->next < 0)
	    break;

	s = &m->slot[s->next];
    }

    return NULL;
}

static inline void map_delete(aoi_space *space, map *m)
{
    space->alloc(space->alloc_ud, m->slot, m->size * sizeof(map_slot), 0);
    space->alloc(space->alloc_ud, m, sizeof(*m), 0);
}

static map *map_new(aoi_space *space)
{
    int i;
    map *m = space->alloc(space->alloc_ud, NULL, 0, sizeof(*m));
    m->size = PRE_ALLOC;
    m->lastfree = PRE_ALLOC - 1;
    m->slot = space->alloc(space->alloc_ud, NULL, 0, m->size * sizeof(map_slot));

    for (i = 0; i < m->size; i++) {
	struct map_slot *s = &m->slot[i];
	s->id = INVALID_ID;
	s->obj = NULL;
	s->next = -1;
    }

    return m;
}

static inline object *grab_object(object *obj)
{
    obj->ref++;
    return obj;
}

static inline void delete_object(aoi_space *space, object *obj)
{
    space->alloc(space->alloc_ud, obj, sizeof(*obj), 0);
}

static inline void drop_object(aoi_space *space, object *obj)
{
    obj->ref--;
    if (obj->ref == 0) {
	map_drop(space->object, obj->id);
	delete_object(space, obj);
    }
}

static inline object_set *set_new(aoi_space *space)
{
    object_set *set = space->alloc(space->alloc_ud, NULL, 0, sizeof(*set));
    set->cap = PRE_ALLOC;
    set->number = 0;
    set->slot = space->alloc(space->alloc_ud, NULL, 0, set->cap * sizeof(object *));

    return set;
}

aoi_space *aoi_create(float radius, aoi_Alloc alloc, void *ud)
{
    aoi_space *space = alloc(ud, NULL, 0, sizeof(*space));
    space->radius = radius;
    space->radius2 = space->radius * space->radius;
    space->alloc = alloc;
    space->alloc_ud = ud;

    space->object = map_new(space);
    space->watcher_static = set_new(space);
    space->marker_static = set_new(space);
    space->watcher_move = set_new(space);
    space->marker_move = set_new(space);
    space->hot = NULL;

    return space;
}

static void delete_pair_list(aoi_space *space)
{
    pair_list *p = space->hot;

    while (p) {
	struct pair_list *next = p->next;
	space->alloc(space->alloc_ud, p, sizeof(*p), 0);
	p = next;
    }
}

static inline void delete_set(aoi_space *space, object_set *set)
{
    space->alloc(space->alloc_ud, set->slot, set->cap * sizeof(object *), 0);
    space->alloc(space->alloc_ud, set, sizeof(*set), 0);
}

void aoi_release(aoi_space *space)
{
    delete_pair_list(space);

    delete_set(space, space->watcher_static);
    delete_set(space, space->marker_static);
    delete_set(space, space->watcher_move);
    delete_set(space, space->marker_move);

    map_foreach(space->object, (void (*)(void *, object *))delete_object, space);
    map_delete(space, space->object);

    space->alloc(space->alloc_ud, space, sizeof(*space), 0);
}

static inline void copy_position(float dst[3], const float src[3])
{
    dst[0] = src[0], dst[1] = src[1], dst[2] = src[2];
}

static inline bool change_mode(object *obj, bool set_watcher, bool set_marker)
{
    bool change = false;

    if (obj->mode == 0) {
	if (set_watcher)
	    obj->mode |= MODE_WATCHER;

	if (set_marker)
	    obj->mode |= MODE_MARKER;

	return true;
    }

    if (set_watcher) {
	if (!(obj->mode & MODE_WATCHER)) {
	    obj->mode |= MODE_WATCHER;
	    change = true;
	}
    } else {
	if (obj->mode & MODE_WATCHER) {
	    obj->mode &= ~MODE_WATCHER;
	    change = true;
	}
    }

    if (set_marker) {
	if (!(obj->mode & MODE_MARKER)) {
	    obj->mode |= MODE_MARKER;
	    change = true;
	}
    } else {
	if (obj->mode & MODE_MARKER) {
	    obj->mode &= ~MODE_MARKER;
	    change = true;
	}
    }

    return change;
}

static inline bool is_near(float radius2, const float p1[3], const float p2[3])
{
    return DIST2(p1, p2) < radius2 * 0.25f;
}

static inline float dist2(const object *p1, const object *p2)
{
    float d = DIST2(p1->position, p2->position);
    return d;
}

void aoi_update(aoi_space *space, uint32_t id, const char *mode, const float pos[3])
{
    object *obj = map_query(space, space->object, id);
    int i;
    bool set_watcher = false;
    bool set_marker = false;

    for (i = 0; mode[i]; i++) {
	char m = mode[i];
	switch (m) {
	case 'w': set_watcher = true; break;
	case 'm': set_marker = true; break;
	case 'd':
	      if (!(obj->mode & MODE_DROP)) {
		  obj->mode = MODE_DROP;
		  drop_object(space, obj);
	      }
	      return;
	}
    }

    if (obj->mode & MODE_DROP) {
	obj->mode &= ~MODE_DROP;
	grab_object(obj);
    }

    bool changed = change_mode(obj, set_watcher, set_marker);

    copy_position(obj->position, pos);
    if (changed || !is_near(space->radius2, obj->last, pos)) {
	// new object or change object mode
	// or position changed
	copy_position(obj->last, obj->position);
	obj->mode |= MODE_MOVE;
	obj->version++;
    }
}

static inline void drop_pair(aoi_space *space, pair_list *p)
{
    drop_object(space, p->watcher);
    drop_object(space, p->marker);
    space->alloc(space->alloc_ud, p, sizeof(*p), 0);
}

static void flush_pair(aoi_space *space, aoi_Callback cb, void *ud)
{
    pair_list **last = &(space->hot);
    pair_list *p = *last;

    while (p) {
	pair_list *next = p->next;
	if (p->watcher->version != p->watcher_version
	    || p->marker->version != p->marker_version
	    || (p->watcher->mode & MODE_DROP)
	    || (p->marker->mode & MODE_DROP)
	   ) {
	    drop_pair(space, p);
	    *last = next;
	} else {
	    float distance2 = dist2(p->watcher, p->marker);
	    if (distance2 > space->radius2 * 4) {
		drop_pair(space, p);
		*last = next;
	    } else if (distance2 < space->radius2) {
		cb(ud, p->watcher->id, p->marker->id);
		drop_pair(space, p);
		*last = next;
	    } else {
		last = &(p->next);
	    }
	}

	p = next;
    }
}

static inline void set_push_back(aoi_space *space, object_set *set, object *obj)
{
    if (set->number >= set->cap) {
	int cap = set->cap * 2;
	set->slot = space->alloc(space->alloc_ud, set->slot, set->cap * sizeof(object *), cap * sizeof(object *));
	set->cap = cap;
    }

    set->slot[set->number++] = obj;
}

static void set_push(void *s, object *obj)
{
    aoi_space *space = (aoi_space *)s;

    int mode = obj->mode;

    if (mode & MODE_WATCHER) {
	if (mode & MODE_MOVE) {
	    set_push_back(space, space->watcher_move, obj);
	    obj->mode &= ~MODE_MOVE;
	} else
	    set_push_back(space, space->watcher_static, obj);
    }

    if (mode & MODE_MARKER) {
	if (mode & MODE_MOVE) {
	    set_push_back(space, space->marker_move, obj);
	    obj->mode &= ~MODE_MOVE;
	} else
	    set_push_back(space, space->marker_static, obj);
    }
}

static inline void gen_pair(aoi_space *space, object *watcher, object *marker, aoi_Callback cb, void *ud)
{
    if (watcher == marker)
	return;

    float distance2 = dist2(watcher, marker);
    if (distance2 < space->radius2) {
	cb(ud, watcher->id, marker->id);
	return;
    }

    if (distance2 > space->radius2 * 4)
	return;

    pair_list *p = space->alloc(space->alloc_ud, NULL, 0, sizeof(*p));
    p->watcher = grab_object(watcher);
    p->watcher_version = watcher->version;
    p->marker = grab_object(marker);
    p->marker_version = marker->version;
    p->next = space->hot;
    space->hot = p;
}

static void gen_pair_list(aoi_space *space, object_set *watcher, object_set *marker, aoi_Callback cb, void *ud)
{
    int i, j;

    for (i = 0; i < watcher->number; i++)
	for (j = 0; j < marker->number; j++)
	    gen_pair(space, watcher->slot[i], marker->slot[j], cb, ud);
}

void aoi_message(aoi_space *space, aoi_Callback cb, void *ud)
{
    flush_pair(space, cb, ud);
    space->watcher_static->number = 0;
    space->watcher_move->number = 0;
    space->marker_static->number = 0;
    space->marker_move->number = 0;
    map_foreach(space->object, set_push, space);
    gen_pair_list(space, space->watcher_static, space->marker_move, cb, ud);
    gen_pair_list(space, space->watcher_move, space->marker_static, cb, ud);
    gen_pair_list(space, space->watcher_move, space->marker_move, cb, ud);
}

static void *default_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    (void)ud; (void)osize; // not used;

    if (nsize == 0) {
	free(ptr);
	return NULL;
    }

    return realloc(ptr, nsize);
}

aoi_space *aoi_new(float radius)
{
    return aoi_create(radius, default_alloc, NULL);
}
