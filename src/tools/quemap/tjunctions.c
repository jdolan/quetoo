/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "qbsp.h"

typedef struct mapDrawVert_s {
	vec3_t xyz;
	int edgeNo;
} mapDrawVert_t;

typedef struct mapDrawSurface_s {
	int numVerts;
	mapDrawVert_t *verts;
	face_t *face;
} mapDrawSurface_t;

typedef struct edgePoint_s {
	float		intercept;
	vec3_t		xyz;
	struct edgePoint_s	*prev, *next;
} edgePoint_t;

typedef struct edgeLine_s {
	vec3_t		normal1;
	float		dist1;
	
	vec3_t		normal2;
	float		dist2;
	
	vec3_t		origin;
	vec3_t		dir;

	edgePoint_t	chain;		// unused element of doubly linked list
} edgeLine_t;

typedef struct {
	float		length;
	mapDrawVert_t	*dv[2];
} originalEdge_t;

#define	MAX_ORIGINAL_EDGES	0x0000
static originalEdge_t	originalEdges[MAX_ORIGINAL_EDGES];
static int				numOriginalEdges;


#define	MAX_EDGE_LINES		0x10000
static edgeLine_t		edgeLines[MAX_EDGE_LINES];
static int				numEdgeLines;

static int				c_degenerateEdges;
static int				c_addedVerts;
static int				c_totalVerts;

static int				c_natural, c_rotate, c_cant;

// these should be whatever epsilon we actually expect,
// plus SNAP_INT_TO_FLOAT 
#define	LINE_POSITION_EPSILON	0.25
#define	POINT_ON_LINE_EPSILON	0.25

/*
====================
InsertPointOnEdge
====================
*/
static void InsertPointOnEdge( vec3_t v, edgeLine_t *e ) {
	vec3_t		delta;
	float		d;
	edgePoint_t	*p, *scan;

	VectorSubtract( v, e->origin, delta );
	d = DotProduct( delta, e->dir );

	p = malloc( sizeof(edgePoint_t) );
	p->intercept = d;
	VectorCopy( v, p->xyz );

	if ( e->chain.next == &e->chain ) {
		e->chain.next = e->chain.prev = p;
		p->next = p->prev = &e->chain;
		return;
	}

	scan = e->chain.next;
	for ( ; scan != &e->chain ; scan = scan->next ) {
		d = p->intercept - scan->intercept;
		if ( d > -LINE_POSITION_EPSILON && d < LINE_POSITION_EPSILON ) {
			free( p );
			return;		// the point is already set
		}

		if ( p->intercept < scan->intercept ) {
			// insert here
			p->prev = scan->prev;
			p->next = scan;
			scan->prev->next = p;
			scan->prev = p;
			return;
		}
	}

	// add at the end
	p->prev = scan->prev;
	p->next = scan;
	scan->prev->next = p;
	scan->prev = p;
}


/*
================
MakeNormalVectors

Given a normalized forward vector, create two
other perpendicular vectors
================
*/
static void MakeNormalVectors( const vec3_t forward, vec3_t right, vec3_t up) {
	float		d;

	// this rotate and negate guarantees a vector
	// not colinear with the original
	right[1] = -forward[0];
	right[2] = forward[1];
	right[0] = forward[2];

	d = DotProduct (right, forward);
	VectorMA (right, -d, forward, right);
	VectorNormalize (right);
	CrossProduct (right, forward, up);
}

/*
====================
AddEdge
====================
*/
static int AddEdge( vec3_t v1, vec3_t v2, _Bool createNonAxial ) {
	int			i;
	edgeLine_t	*e;
	float		d;
	vec3_t		dir;

	VectorSubtract( v2, v1, dir );
	d = VectorNormalize( dir );
	if ( d < 0.1 ) {
		// if we added a 0 length vector, it would make degenerate planes
		c_degenerateEdges++;
		return -1;
	}

	if ( !createNonAxial ) {
		if ( fabs( dir[0] + dir[1] + dir[2] ) != 1.0 ) {
			if ( numOriginalEdges == MAX_ORIGINAL_EDGES ) {
				Com_Error(ERROR_FATAL, "MAX_ORIGINAL_EDGES\n" );
			}
			originalEdges[ numOriginalEdges ].dv[0] = (mapDrawVert_t *)v1;
			originalEdges[ numOriginalEdges ].dv[1] = (mapDrawVert_t *)v2;
			originalEdges[ numOriginalEdges ].length = d;
			numOriginalEdges++;
			return -1;
		}
	}

	for ( i = 0 ; i < numEdgeLines ; i++ ) {
		e = &edgeLines[i];

		d = DotProduct( v1, e->normal1 ) - e->dist1;
		if ( d < -POINT_ON_LINE_EPSILON || d > POINT_ON_LINE_EPSILON ) {
			continue;
		}
		d = DotProduct( v1, e->normal2 ) - e->dist2;
		if ( d < -POINT_ON_LINE_EPSILON || d > POINT_ON_LINE_EPSILON ) {
			continue;
		}

		d = DotProduct( v2, e->normal1 ) - e->dist1;
		if ( d < -POINT_ON_LINE_EPSILON || d > POINT_ON_LINE_EPSILON ) {
			continue;
		}
		d = DotProduct( v2, e->normal2 ) - e->dist2;
		if ( d < -POINT_ON_LINE_EPSILON || d > POINT_ON_LINE_EPSILON ) {
			continue;
		}

		// this is the edge
		InsertPointOnEdge( v1, e );
		InsertPointOnEdge( v2, e );
		return i;
	}

	// create a new edge
	if ( numEdgeLines >= MAX_EDGE_LINES ) {
		Com_Error( ERROR_DROP, "MAX_EDGE_LINES\n" );
	}

	e = &edgeLines[ numEdgeLines ];
	numEdgeLines++;

	e->chain.next = e->chain.prev = &e->chain;

	VectorCopy( v1, e->origin );
	VectorCopy( dir, e->dir );

	MakeNormalVectors( e->dir, e->normal1, e->normal2 );
	e->dist1 = DotProduct( e->origin, e->normal1 );
	e->dist2 = DotProduct( e->origin, e->normal2 );

	InsertPointOnEdge( v1, e );
	InsertPointOnEdge( v2, e );

	return numEdgeLines - 1;
}

/*
====================
AddSurfaceEdges
====================
*/
static void AddSurfaceEdges( mapDrawSurface_t *ds ) {
	int		i;

	for ( i = 0 ; i < ds->numVerts ; i++ ) {
		// save the edge number in the lightmap field
		// so we don't need to look it up again
		ds->verts[i].edgeNo = 
			AddEdge( ds->verts[i].xyz, ds->verts[(i+1) % ds->numVerts].xyz, false );
	}
}

/*
====================
FixSurfaceJunctions
====================
*/
#define	MAX_SURFACE_VERTS	256
static void FixSurfaceJunctions( mapDrawSurface_t *ds ) {
	int			i, j, k;
	edgeLine_t	*e;
	edgePoint_t	*p;
	int			originalVerts;
	int			counts[MAX_SURFACE_VERTS];
	int			originals[MAX_SURFACE_VERTS];
	int			firstVert[MAX_SURFACE_VERTS];
	mapDrawVert_t	verts[MAX_SURFACE_VERTS], *v1, *v2;
	int			numVerts;
	float		start, end;
	vec3_t		delta;

	originalVerts = ds->numVerts;

	numVerts = 0;
	for ( i = 0 ; i < ds->numVerts ; i++ ) {
		counts[i] = 0;
		firstVert[i] = numVerts;

		// copy first vert
		if ( numVerts == MAX_SURFACE_VERTS ) {
			Com_Error( ERROR_DROP, "MAX_SURFACE_VERTS\n" );
		}
		verts[numVerts] = ds->verts[i];
		originals[numVerts] = i;
		numVerts++;

		// check to see if there are any t junctions before the next vert
		v1 = &ds->verts[i];
		v2 = &ds->verts[ (i+1) % ds->numVerts ];

		j = (int)ds->verts[i].edgeNo;
		if ( j == -1 ) {
			continue;		// degenerate edge
		}
		e = &edgeLines[ j ];
		
		VectorSubtract( v1->xyz, e->origin, delta );
		start = DotProduct( delta, e->dir );

		VectorSubtract( v2->xyz, e->origin, delta );
		end = DotProduct( delta, e->dir );


		if ( start < end ) {
			p = e->chain.next;
		} else {
			p = e->chain.prev;
		}

		for (  ; p != &e->chain ;  ) {
			if ( start < end ) {
				if ( p->intercept > end - ON_EPSILON ) {
					break;
				}
			} else {
				if ( p->intercept < end + ON_EPSILON ) {
					break;
				}
			}

			if ( 
				( start < end && p->intercept > start + ON_EPSILON ) ||
				( start > end && p->intercept < start - ON_EPSILON ) ) {
				// insert this point
				if ( numVerts == MAX_SURFACE_VERTS ) {
					Com_Error(ERROR_FATAL, "MAX_SURFACE_VERTS\n" );
				}

				// take the exact intercept point
				VectorCopy( p->xyz, verts[ numVerts ].xyz );

				// copy the normal
				//VectorCopy( v1->normal, verts[ numVerts ].normal );

				// interpolate the texture coordinates
				/*frac = ( p->intercept - start ) / ( end - start );
				for ( j = 0 ; j < 2 ; j++ ) {
					verts[ numVerts ].st[j] = v1->st[j] + 
						frac * ( v2->st[j] - v1->st[j] );
				}*/

				originals[numVerts] = i;
				numVerts++;
				counts[i]++;
			}

			if ( start < end ) {
				p = p->next;
			} else {
				p = p->prev;
			}
		}
	}

	c_addedVerts += numVerts - ds->numVerts;
	c_totalVerts += numVerts;


	// FIXME: check to see if the entire surface degenerated
	// after snapping

	// rotate the points so that the initial vertex is between
	// two non-subdivided edges
	for ( i = 0 ; i < numVerts ; i++ ) {
		if ( originals[ (i+1) % numVerts ] == originals[ i ] ) {
			continue;
		}
		j = (i + numVerts - 1 ) % numVerts;
		k = (i + numVerts - 2 ) % numVerts;
		if ( originals[ j ] == originals[ k ] ) {
			continue;
		}
		break;
	}

	if ( i == 0 ) {
		// fine the way it is
		c_natural++;

		ds->numVerts = numVerts;
		ds->verts = malloc( numVerts * sizeof( *ds->verts ) );
		memcpy( ds->verts, verts, numVerts * sizeof( *ds->verts ) );

		return;
	}
	if ( i == numVerts ) {
		// create a vertex in the middle to start the fan
		c_cant++;

/*
		memset ( &verts[numVerts], 0, sizeof( verts[numVerts] ) );
		for ( i = 0 ; i < numVerts ; i++ ) {
			for ( j = 0 ; j < 10 ; j++ ) {
				verts[numVerts].xyz[j] += verts[i].xyz[j];
			}
		}
		for ( j = 0 ; j < 10 ; j++ ) {
			verts[numVerts].xyz[j] /= numVerts;
		}

		i = numVerts;
		numVerts++;
*/
	} else {
		// just rotate the vertexes
		c_rotate++;

	}

	ds->numVerts = numVerts;
	ds->verts = malloc( numVerts * sizeof( *ds->verts ) );

	for ( j = 0 ; j < ds->numVerts ; j++ ) {
		ds->verts[j] = verts[ ( j + i ) % ds->numVerts ];
	}
}

/*
================
EdgeCompare
================
*/
static int EdgeCompare( const void *elem1, const void *elem2 ) {
	float	d1, d2;

	d1 = ((originalEdge_t *)elem1)->length;
	d2 = ((originalEdge_t *)elem2)->length;

	if ( d1 < d2 ) {
		return -1;
	}
	if ( d2 > d1 ) {
		return 1;
	}
	return 0;
}


static mapDrawSurface_t surfs[MAX_BSP_FACES];
static int numDrawSurfs = 0;

/**
 * @brief
 */
static void CreateSurfs_r(node_t *node) {
	int32_t i;
	face_t *f;

	if (node->plane_num == PLANENUM_LEAF) {
		return;
	}

	for (f = node->faces; f; f = f->next) {
		mapDrawSurface_t *surf = &surfs[numDrawSurfs];

		surf->numVerts = f->w->num_points;
		surf->verts = malloc(sizeof(mapDrawVert_t) * surf->numVerts);
		surf->face = f;

		for (int j = 0; j < surf->numVerts; j++)
		{
			VectorCopy(f->w->points[j], surf->verts[j].xyz);
			surf->verts[j].edgeNo = 0;
		}

		numDrawSurfs++;
	}

	for (i = 0; i < 2; i++) {
		CreateSurfs_r(node->children[i]);
	}
}

/*
================
FixTJunctions

Call after the surface list has been pruned, but before lightmap allocation
================
*/
void FixTJunctionsQ3( node_t *node ) {
	int					i;
	mapDrawSurface_t	*ds;
	int					axialEdgeLines;
	originalEdge_t		*e;

	Com_Verbose("----- FixTJunctions -----\n");

	numEdgeLines = 0;
	numOriginalEdges = 0;

	// convert nodes to draw surfaces
	i = 0;

	memset(surfs, 0, sizeof(surfs));
	numDrawSurfs = 0;

	CreateSurfs_r(node);

	// add all the edges
	// this actually creates axial edges, but it
	// only creates originalEdge_t structures
	// for non-axial edges

	for ( i = 0 ; i < numDrawSurfs ; i++ ) {
		ds = &surfs[i];

		AddSurfaceEdges( ds );
	}

	axialEdgeLines = numEdgeLines;

	// sort the non-axial edges by length
	qsort( originalEdges, numOriginalEdges, sizeof(originalEdges[0]), EdgeCompare );

	// add the non-axial edges, longest first
	// this gives the most accurate edge description
	for ( i = 0 ; i < numOriginalEdges ; i++ ) {
		e = &originalEdges[i];
		e->dv[0]->edgeNo = AddEdge( e->dv[0]->xyz, e->dv[1]->xyz, true );
	}

	Com_Verbose( "%6i axial edge lines\n", axialEdgeLines );
	Com_Verbose( "%6i non-axial edge lines\n", numEdgeLines - axialEdgeLines );
	Com_Verbose( "%6i degenerate edges\n", c_degenerateEdges );

	// insert any needed vertexes
	for ( i = 0 ; i < numDrawSurfs ; i++ ) {
		ds = &surfs[i];

		FixSurfaceJunctions( ds );
	}

	// convert back to Q2
	for ( i = 0; i < numDrawSurfs ; i++ ) {
		ds = &surfs[i];

		FreeWinding(ds->face->w);
		ds->face->w = AllocWinding(ds->numVerts);

		ds->face->w->num_points = ds->numVerts;
		for (int j = 0; j < ds->numVerts; j++)
			VectorCopy(ds->verts[j].xyz, ds->face->w->points[j]);
	}

	Com_Verbose( "%6i verts added for tjunctions\n", c_addedVerts );
	Com_Verbose( "%6i total verts\n", c_totalVerts );
	Com_Verbose( "%6i naturally ordered\n", c_natural );
	Com_Verbose( "%6i rotated orders\n", c_rotate );
	Com_Verbose( "%6i can't order\n", c_cant );
}
