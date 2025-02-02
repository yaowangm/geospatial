/**********************************************************************
 *
 * PostGIS - Spatial Types for PostgreSQL
 * http://postgis.net
 *
 * PostGIS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * PostGIS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PostGIS.  If not, see <http://www.gnu.org/licenses/>.
 *
 **********************************************************************
 *
 * Copyright (C) 2001-2006 Refractions Research Inc.
 * Modifications Copyright (c) 2017 - Present Pivotal Software, Inc. All Rights Reserved.
 *
 **********************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "liblwgeom_internal.h"
#include "lwgeom_log.h"


LWTIN* lwtin_add_lwtriangle(LWTIN *mobj, const LWTRIANGLE *obj)
{
	return (LWTIN*)lwcollection_add_lwgeom((LWCOLLECTION*)mobj, (LWGEOM*)obj);
}

void lwtin_free(LWTIN *tin)
{
	uint32_t i;
	if ( ! tin ) return;
	if ( tin->bbox )
		lwfree(tin->bbox);

	for ( i = 0; i < tin->ngeoms; i++ )
		if ( tin->geoms && tin->geoms[i] )
			lwtriangle_free(tin->geoms[i]);

	if ( tin->geoms )
		lwfree(tin->geoms);

	lwfree(tin);
}


void printLWTIN(LWTIN *tin)
{
	uint32_t i;
	LWTRIANGLE *triangle;

	if (tin->type != TINTYPE)
		lwerror("printLWTIN called with something else than a TIN");

	lwnotice("LWTIN {");
	lwnotice("    ndims = %i", (int)FLAGS_NDIMS(tin->flags));
	lwnotice("    SRID = %i", (int)tin->srid);
	lwnotice("    ngeoms = %i", (int)tin->ngeoms);

	for (i=0; i<tin->ngeoms; i++)
	{
		triangle = (LWTRIANGLE *) tin->geoms[i];
		printPA(triangle->points);
	}
	lwnotice("}");
}


/*
 * TODO rewrite all this stuff to be based on a truly topological model
 */

struct struct_tin_arcs
{
	double ax, ay, az;
	double bx, by, bz;
	uint32_t cnt, face;
};
typedef struct struct_tin_arcs *tin_arcs;

/* We supposed that the geometry is valid
   we could have wrong result if not */
int lwtin_is_closed(const LWTIN *tin)
{
	uint32_t i, j, k;
	uint32_t narcs, carc;
	int found;
	tin_arcs arcs;
	POINT4D pa, pb;
	LWTRIANGLE *patch;

	/* If surface is not 3D, it's can't be closed */
	if (!FLAGS_GET_Z(tin->flags)) return 0;

	/* Max theoretical arcs number if no one is shared ... */
	narcs = 3 * tin->ngeoms;

	arcs = lwalloc(sizeof(struct struct_tin_arcs) * narcs);
	for (i=0, carc=0; i < tin->ngeoms ; i++)
	{

		patch = (LWTRIANGLE *) tin->geoms[i];
		for (j=0; j < 3 ; j++)
		{

			getPoint4d_p(patch->points, j,   &pa);
			getPoint4d_p(patch->points, j+1, &pb);

			/* Make sure to order the 'lower' point first */
			if ( (pa.x > pb.x) ||
			        (pa.x == pb.x && pa.y > pb.y) ||
			        (pa.x == pb.x && pa.y == pb.y && pa.z > pb.z) )
			{
				pa = pb;
				getPoint4d_p(patch->points, j, &pb);
			}

			for (found=0, k=0; k < carc ; k++)
			{

				if (  ( arcs[k].ax == pa.x && arcs[k].ay == pa.y &&
				        arcs[k].az == pa.z && arcs[k].bx == pb.x &&
				        arcs[k].by == pb.y && arcs[k].bz == pb.z &&
				        arcs[k].face != i) )
				{
					arcs[k].cnt++;
					found = 1;

					/* Look like an invalid TIN
					      anyway not a closed one */
					if (arcs[k].cnt > 2)
					{
						lwfree(arcs);
						return 0;
					}
				}
			}

			if (!found)
			{
				arcs[carc].cnt=1;
				arcs[carc].face=i;
				arcs[carc].ax = pa.x;
				arcs[carc].ay = pa.y;
				arcs[carc].az = pa.z;
				arcs[carc].bx = pb.x;
				arcs[carc].by = pb.y;
				arcs[carc].bz = pb.z;
				carc++;

				/* Look like an invalid TIN
				      anyway not a closed one */
				if (carc > narcs)
				{
					lwfree(arcs);
					return 0;
				}
			}
		}
	}

	/* A TIN is closed if each edge
	       is shared by exactly 2 faces */
	for (k=0; k < carc ; k++)
	{
		if (arcs[k].cnt != 2)
		{
			lwfree(arcs);
			return 0;
		}
	}
	lwfree(arcs);

	/* Invalid TIN case */
	if (carc < tin->ngeoms) return 0;

	return 1;
}
