#pragma once

#define CHAR_WIDTH      8
#define CHAR_HEIGHT     16

#define CHAR_SPACING_X  8 
#define CHAR_SPACING_Y  20



#define MIN(X, Y) ({\
	auto __X__ = (X);\
	auto __Y__ = (Y);\
	(__X__ < __Y__)? __X__ : __Y__;\
})

#define MAX(X, Y) ({\
	auto __X__ = (X);\
	auto __Y__ = (Y);\
	(__X__ > __Y__)? __X__ : __Y__;\
})

struct Point {
	int x, y;
};
struct Rect {
	int l, t, r, b; // left, top, right, bottom
};


static void clamp_in_to_rect(int &x, int &y, Rect *rect) {
	x = (x < rect->l)? rect->l: ((x >= rect->r)? rect->r: x);
	y = (y < rect->t)? rect->t: ((y >= rect->b)? rect->b: y);
}
	
static bool rect_intersect(const Rect *src, Rect *dst) {
	int l = MAX(src->l, dst->l);	
	int r = MIN(src->r, dst->r);	
	if (r < l) return false;

	int t = MAX(src->t, dst->t);	
	int b = MIN(src->b, dst->b);	
	if (b < t) return false;

	dst->l = l;
	dst->t = t;
	dst->r = r;
	dst->b = b;
	return true;
}

static int rect_union(const Rect *A, const Rect *B, Rect (&dst)[3]) {
// returns up to 3 non-overlapping rects forming the union of A and B
	dst[0] = *B;
	if (!rect_intersect(A, &dst[0])) {
		dst[0] = *A;
		dst[1] = *B;
		return 2;
	}

	const Rect *hi;
	const Rect *lo;
	if (A->t < B->t) {
		hi = A;
		lo = B;
	} else {
		hi = B;
		lo = A;
	}

	dst[0].l = hi->l;
	dst[0].t = hi->t;
	dst[0].r = hi->r;
	dst[0].b = lo->t;
	
	dst[1].t = lo->t;
	if (A->b < B->b) {
		hi = A;
		lo = B;
	} else {
		hi = B;
		lo = A;
	}
		
	dst[1].l = (A->l < B->l) ? A->l: B->l;
	dst[1].r = (A->r > B->r) ? A->r: B->r;
	dst[1].b = hi->b;
	
	dst[2].l = lo->l;
	dst[2].t = hi->b;
	dst[2].r = lo->r;
	dst[2].b = lo->b;

	return 3;
}
