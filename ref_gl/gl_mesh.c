/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2024 Kleadron Software

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// gl_mesh.c: triangle model functions

#include "gl_local.h"

/*
=============================================================

  ALIAS MODELS

=============================================================
*/

#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

typedef float vec4_t[4];

static	vec4_t	s_lerped[MAX_VERTS];
//static	vec3_t	lerped[MAX_VERTS];
static	vec4_t	s_lerpednormals[MAX_VERTS];

vec3_t	shadevector;
float	shadelight[3];

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anormtab.h"
;

float	*shadedots = r_avertexnormal_dots[0];

void GL_LerpVerts( int nverts, dtrivertx_t *v, dtrivertx_t *ov, dtrivertx_t *verts, float *lerp, float move[3], float frontv[3], float backv[3] )
{
	int i;

	//PMM -- added RF_SHELL_DOUBLE, RF_SHELL_HALF_DAM
	if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) )
	{
		for (i=0 ; i < nverts; i++, v++, ov++, lerp+=4 )
		{
			float *normal = r_avertexnormals[verts[i].lightnormalindex];

			lerp[0] = move[0] + ov->v[0]*backv[0] + v->v[0]*frontv[0] + normal[0] * POWERSUIT_SCALE;
			lerp[1] = move[1] + ov->v[1]*backv[1] + v->v[1]*frontv[1] + normal[1] * POWERSUIT_SCALE;
			lerp[2] = move[2] + ov->v[2]*backv[2] + v->v[2]*frontv[2] + normal[2] * POWERSUIT_SCALE; 
		}
	}
	else
	{
		for (i=0 ; i < nverts; i++, v++, ov++, lerp+=4)
		{
			lerp[0] = move[0] + ov->v[0]*backv[0] + v->v[0]*frontv[0];
			lerp[1] = move[1] + ov->v[1]*backv[1] + v->v[1]*frontv[1];
			lerp[2] = move[2] + ov->v[2]*backv[2] + v->v[2]*frontv[2];
		}
	}

}

void GL_LerpVerts_HD(int nverts, dtrivertx_t *v, dtrivertx_t *ov, dtrivertx_t *verts, float *lerp, float move[3], float frontv[3], float backv[3])
{
	int i;

	// these are fractional values apparently
	byte *v16 = (byte*)(((byte*)v) + (nverts * 4));
	byte *ov16 = (byte*)(((byte*)ov) + (nverts * 4));
	// fractional scale
	float s = 1.0f / 256.0f;

	//PMM -- added RF_SHELL_DOUBLE, RF_SHELL_HALF_DAM
	if (currententity->flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM))
	{
		for (i = 0; i < nverts; i++, v++, ov++, v16+=3, ov16+=3, lerp += 4)
		{
			float *normal = r_avertexnormals[verts[i].lightnormalindex];

			lerp[0] = move[0] + ((float)ov->v[0] + (float)ov16[0] * s) * backv[0] + ((float)v->v[0] + (float)v16[0] * s) * frontv[0] + normal[0] * POWERSUIT_SCALE;
			lerp[1] = move[1] + ((float)ov->v[1] + (float)ov16[1] * s) * backv[1] + ((float)v->v[1] + (float)v16[1] * s) * frontv[1] + normal[1] * POWERSUIT_SCALE;
			lerp[2] = move[2] + ((float)ov->v[2] + (float)ov16[2] * s) * backv[2] + ((float)v->v[2] + (float)v16[2] * s) * frontv[2] + normal[2] * POWERSUIT_SCALE;
		}
	}
	else
	{
		for (i = 0; i < nverts; i++, v++, ov++, v16 += 3, ov16 += 3, lerp += 4)
		{
			lerp[0] = move[0] + ((float)ov->v[0] + (float)ov16[0] * s) * backv[0] + ((float)v->v[0] + (float)v16[0] * s) * frontv[0];
			lerp[1] = move[1] + ((float)ov->v[1] + (float)ov16[1] * s) * backv[1] + ((float)v->v[1] + (float)v16[1] * s) * frontv[1];
			lerp[2] = move[2] + ((float)ov->v[2] + (float)ov16[2] * s) * backv[2] + ((float)v->v[2] + (float)v16[2] * s) * frontv[2];
		}
	}

}

void GL_LerpNormals(int nverts, dtrivertx_t *v, dtrivertx_t *ov, dtrivertx_t *verts, float *lerp, float front, float back)
{
	int i;

	for (i = 0; i < nverts; i++, v++, ov++, lerp += 4)
	{
		float *oldnormal = r_avertexnormals[ov->lightnormalindex];
		float *normal = r_avertexnormals[v->lightnormalindex];
		lerp[0] = (oldnormal[0] * back) + (normal[0] * front);
		lerp[1] = (oldnormal[1] * back) + (normal[1] * front);
		lerp[2] = (oldnormal[2] * back) + (normal[2] * front);

		// normalize normal!
		float length, ilength;

		length = lerp[0] * lerp[0] + lerp[1] * lerp[1] + lerp[2] * lerp[2];
		length = sqrt(length);

		if (length)
		{
			ilength = 1 / length;
			lerp[0] *= ilength;
			lerp[1] *= ilength;
			lerp[2] *= ilength;
		}
	}
}

/*
=============
GL_DrawAliasFrameLerp

interpolates between two frames and origins
FIXME: batch lerp all vertexes
=============
*/
void GL_DrawAliasFrameLerp (dmdl_t *paliashdr, float backlerp, qboolean calcnormals, qboolean unlit)
{
	float 	l;
	daliasframe_t	*frame, *oldframe;
	dtrivertx_t	*v, *ov, *verts;
	int		*order;
	int		count;
	float	frontlerp;
	float	alpha;
	vec3_t	move, delta, vectors[3];
	vec3_t	frontv, backv;
	int		i;
	int		index_xyz;
	float	*lerp;
	float	*lerpnormals;

	frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames 
		+ currententity->frame * paliashdr->framesize);
	verts = v = frame->verts;

	oldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames 
		+ currententity->oldframe * paliashdr->framesize);
	ov = oldframe->verts;

	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);

//	glTranslatef (frame->translate[0], frame->translate[1], frame->translate[2]);
//	glScalef (frame->scale[0], frame->scale[1], frame->scale[2]);

	if (currententity->flags & RF_TRANSLUCENT)
		alpha = currententity->alpha;
	else
		alpha = 1.0;

	// PMM - added double shell
	if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) )
		qglDisable( GL_TEXTURE_2D );

	frontlerp = 1.0 - backlerp;

	// move should be the delta back to the previous frame * backlerp
	VectorSubtract (currententity->oldorigin, currententity->origin, delta);
	AngleVectors (currententity->angles, vectors[0], vectors[1], vectors[2]);

	move[0] = DotProduct (delta, vectors[0]);	// forward
	move[1] = -DotProduct (delta, vectors[1]);	// left
	move[2] = DotProduct (delta, vectors[2]);	// up

	VectorAdd (move, oldframe->translate, move);

	for (i=0 ; i<3 ; i++)
	{
		move[i] = backlerp*move[i] + frontlerp*frame->translate[i];
	}

	for (i = 0; i < 3; i++)
	{
		frontv[i] = frontlerp * frame->scale[i];
		backv[i] = backlerp * oldframe->scale[i];
	}

	lerp = s_lerped[0];
	lerpnormals = s_lerpednormals[0];

	if (calcnormals)
	{
		GL_LerpNormals(paliashdr->num_xyz, v, ov, verts, lerpnormals, frontlerp, backlerp);
	}

	if (currentmodel->is_hd)
	{
		GL_LerpVerts_HD(paliashdr->num_xyz, v, ov, verts, lerp, move, frontv, backv);
	}
	else
	{
		GL_LerpVerts(paliashdr->num_xyz, v, ov, verts, lerp, move, frontv, backv);
	}
	

	if ( gl_vertex_arrays->value )
	{
		float colorArray[MAX_VERTS*4];
		float normalArray[MAX_VERTS*4];

		qglEnableClientState( GL_VERTEX_ARRAY );
		qglVertexPointer( 3, GL_FLOAT, 16, s_lerped );	// padded for SIMD

//		if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE ) )

		// unlit is just a white color
		if (unlit)
		{
			qglColor4f(1.0f, 1.0f, 1.0f, alpha);
		}
		// PMM - added double damage shell
		else if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) )
		{
			qglColor4f( shadelight[0], shadelight[1], shadelight[2], alpha );
		}
		else
		{
			qglEnableClientState( GL_COLOR_ARRAY );
			qglColorPointer( 3, GL_FLOAT, 0, colorArray );

			//
			// pre light everything
			//
			for ( i = 0; i < paliashdr->num_xyz; i++ )
			{
				float l = shadedots[verts[i].lightnormalindex];

				colorArray[i*3+0] = l * shadelight[0];
				colorArray[i*3+1] = l * shadelight[1];
				colorArray[i*3+2] = l * shadelight[2];
			}
		}

		if (calcnormals)
		{
			qglEnableClientState(GL_NORMAL_ARRAY);
			qglNormalPointer(GL_FLOAT, 16, lerpnormals);

			//for (i = 0; i < paliashdr->num_xyz; i++)
			//{
			//	float *n = r_avertexnormals[verts[i].lightnormalindex];

			//	normalArray[i * 3 + 0] = n[0];
			//	normalArray[i * 3 + 1] = n[1];
			//	normalArray[i * 3 + 2] = n[2];
			//}
		}

		if ( qglLockArraysEXT != 0 )
			qglLockArraysEXT( 0, paliashdr->num_xyz );

		while (1)
		{
			// get the vertex count and primitive type
			count = *order++;
			if (!count)
				break;		// done
			if (count < 0)
			{
				count = -count;
				qglBegin (GL_TRIANGLE_FAN);
			}
			else
			{
				qglBegin (GL_TRIANGLE_STRIP);
			}

			// PMM - added double damage shell
			if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) )
			{
				do
				{
					index_xyz = order[2];
					order += 3;

					qglVertex3fv( s_lerped[index_xyz] );

				} while (--count);
			}
			else
			{
				do
				{
					// texture coordinates come from the draw list
					qglTexCoord2f (((float *)order)[0], ((float *)order)[1]);
					index_xyz = order[2];

					order += 3;

					// normals and vertexes come from the frame list
//					l = shadedots[verts[index_xyz].lightnormalindex];
					
//					qglColor4f (l* shadelight[0], l*shadelight[1], l*shadelight[2], alpha);
					qglArrayElement( index_xyz );

				} while (--count);
			}
			qglEnd ();
		}

		if ( qglUnlockArraysEXT != 0 )
			qglUnlockArraysEXT();

		if (calcnormals)
		{
			qglDisableClientState(GL_NORMAL_ARRAY);
		}

		qglDisableClientState(GL_VERTEX_ARRAY);
		qglDisableClientState(GL_COLOR_ARRAY);
	}
	else
	{
		while (1)
		{
			// get the vertex count and primitive type
			count = *order++;
			if (!count)
				break;		// done
			if (count < 0)
			{
				count = -count;
				qglBegin (GL_TRIANGLE_FAN);
			}
			else
			{
				qglBegin (GL_TRIANGLE_STRIP);
			}

			if (unlit)
			{
				do
				{
					// texture coordinates come from the draw list
					qglTexCoord2f(((float *)order)[0], ((float *)order)[1]);
					index_xyz = order[2];
					order += 3;

					qglColor4f(1.0f, 1.0f, 1.0f, alpha);
					qglVertex3fv(s_lerped[index_xyz]);
				} while (--count);
			}
			else if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE ) )
			{
				do
				{
					index_xyz = order[2];
					order += 3;

					qglColor4f( shadelight[0], shadelight[1], shadelight[2], alpha);
					qglVertex3fv (s_lerped[index_xyz]);

				} while (--count);
			}
			else
			{
				do
				{
					// texture coordinates come from the draw list
					qglTexCoord2f (((float *)order)[0], ((float *)order)[1]);
					index_xyz = order[2];
					order += 3;

					// normals and vertexes come from the frame list
					l = shadedots[verts[index_xyz].lightnormalindex];
					
					qglColor4f (l* shadelight[0], l*shadelight[1], l*shadelight[2], alpha);
					if (calcnormals)
					{
						qglNormal3fv(s_lerpednormals[index_xyz]);
					}
					qglVertex3fv (s_lerped[index_xyz]);
				} while (--count);
			}

			qglEnd ();
		}
	}

//	if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE ) )
	// PMM - added double damage shell
	if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) )
		qglEnable( GL_TEXTURE_2D );
}


/*
=============
GL_DrawAliasShadow
=============
*/
extern	vec3_t			lightspot;

void GL_DrawAliasShadow (entity_t *e, dmdl_t *paliashdr, int posenum)
{
	dtrivertx_t	*verts;
	int		*order;
	vec3_t	point;
	float	height, lheight;
	int		count;
	daliasframe_t	*frame;

	lheight = currententity->origin[2] - lightspot[2];

	frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames 
		+ currententity->frame * paliashdr->framesize);
	verts = frame->verts;

//	height = 0;
	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);
	height = -lheight + 0.1f; // was 1.0f, lowered shadows to ground more - MrG

	// Knightmare- don't draw shadow above entity
	if ((currententity->origin[2]+height) > currententity->origin[2])
		return;

	// Knightmare- don't draw shadows above view origin
	if (r_newrefdef.vieworg[2] < (currententity->origin[2] + height))
		return;

	qglPushMatrix ();
	R_RotateForEntity (e, false);
	qglDisable (GL_TEXTURE_2D);
	qglEnable (GL_BLEND);
	qglColor4f (0, 0, 0, gl_shadowalpha->value); // was 0.5

	// Knightmare- Stencil shadows by MrG
	if (gl_config.have_stencil)
	{
		qglEnable(GL_STENCIL_TEST);
		qglStencilFunc(GL_EQUAL, 1, 2);
		qglStencilOp(GL_KEEP,GL_KEEP,GL_INCR);
	}
	// End Stencil shadows - MrG

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			qglBegin (GL_TRIANGLE_FAN);
		}
		else
			qglBegin (GL_TRIANGLE_STRIP);

		do
		{
			// normals and vertexes come from the frame list
/*
			point[0] = verts[order[2]].v[0] * frame->scale[0] + frame->translate[0];
			point[1] = verts[order[2]].v[1] * frame->scale[1] + frame->translate[1];
			point[2] = verts[order[2]].v[2] * frame->scale[2] + frame->translate[2];
*/

			memcpy( point, s_lerped[order[2]], sizeof( point )  );

			point[0] -= shadevector[0]*(point[2]+lheight);
			point[1] -= shadevector[1]*(point[2]+lheight);
			point[2] = height;
//			height -= 0.001;
			qglVertex3fv (point);

			order += 3;

//			verts++;

		} while (--count);

		qglEnd ();
	}

	// Knightmare- disable Stencil shadows
	if (gl_config.have_stencil)
		qglDisable(GL_STENCIL_TEST);
	
	qglColor4f (1,1,1,1);
	qglEnable (GL_TEXTURE_2D);
	qglDisable (GL_BLEND);
	qglPopMatrix ();
}


/*
** R_CullAliasModel
*/
static qboolean R_CullAliasModel( vec3_t bbox[8], entity_t *e )
{
	int i;
	vec3_t		mins, maxs;
	dmdl_t		*paliashdr;
	vec3_t		vectors[3];
	vec3_t		thismins, oldmins, thismaxs, oldmaxs;
	daliasframe_t *pframe, *poldframe;
	vec3_t angles;

	paliashdr = (dmdl_t *)currentmodel->extradata;

	if ( ( e->frame >= paliashdr->num_frames ) || ( e->frame < 0 ) )
	{
		ri.Con_Printf (PRINT_ALL, "R_CullAliasModel %s: no such frame %d\n", 
			currentmodel->name, e->frame);
		e->frame = 0;
	}
	if ( ( e->oldframe >= paliashdr->num_frames ) || ( e->oldframe < 0 ) )
	{
		ri.Con_Printf (PRINT_ALL, "R_CullAliasModel %s: no such oldframe %d\n", 
			currentmodel->name, e->oldframe);
		e->oldframe = 0;
	}

	pframe = ( daliasframe_t * ) ( ( byte * ) paliashdr + 
		                              paliashdr->ofs_frames +
									  e->frame * paliashdr->framesize);

	poldframe = ( daliasframe_t * ) ( ( byte * ) paliashdr + 
		                              paliashdr->ofs_frames +
									  e->oldframe * paliashdr->framesize);

	/*
	** compute axially aligned mins and maxs
	*/
	if ( pframe == poldframe )
	{
		for ( i = 0; i < 3; i++ )
		{
			mins[i] = pframe->translate[i];
			maxs[i] = mins[i] + pframe->scale[i]*255;
		}
	}
	else
	{
		for ( i = 0; i < 3; i++ )
		{
			thismins[i] = pframe->translate[i];
			thismaxs[i] = thismins[i] + pframe->scale[i]*255;

			oldmins[i]  = poldframe->translate[i];
			oldmaxs[i]  = oldmins[i] + poldframe->scale[i]*255;

			if ( thismins[i] < oldmins[i] )
				mins[i] = thismins[i];
			else
				mins[i] = oldmins[i];

			if ( thismaxs[i] > oldmaxs[i] )
				maxs[i] = thismaxs[i];
			else
				maxs[i] = oldmaxs[i];
		}
	}

#if 1	// Knightmare- jitspore's fix for correct bbox rotation
	//
	// compute bounding box and rotate
	//
	VectorCopy( e->angles, angles );
	AngleVectors( angles, vectors[0], vectors[1], vectors[2] );
	VectorSubtract(vec3_origin, vectors[1], vectors[1]); // AngleVectors returns "right" instead of "left"
	for (i = 0; i < 8; i++)
	{
		vec3_t   tmp;
		tmp[0] = ((i & 1) ? mins[0] : maxs[0]);
		tmp[1] = ((i & 2) ? mins[1] : maxs[1]);
		tmp[2] = ((i & 4) ? mins[2] : maxs[2]);

		bbox[i][0] = vectors[0][0] * tmp[0] + vectors[1][0] * tmp[1] + vectors[2][0] * tmp[2] + e->origin[0];
		bbox[i][1] = vectors[0][1] * tmp[0] + vectors[1][1] * tmp[1] + vectors[2][1] * tmp[2] + e->origin[1];
		bbox[i][2] = vectors[0][2] * tmp[0] + vectors[1][2] * tmp[1] + vectors[2][2] * tmp[2] + e->origin[2];
	}
#else
	/*
	** compute a full bounding box
	*/
	for ( i = 0; i < 8; i++ )
	{
		vec3_t   tmp;

		if ( i & 1 )
			tmp[0] = mins[0];
		else
			tmp[0] = maxs[0];

		if ( i & 2 )
			tmp[1] = mins[1];
		else
			tmp[1] = maxs[1];

		if ( i & 4 )
			tmp[2] = mins[2];
		else
			tmp[2] = maxs[2];

		VectorCopy( tmp, bbox[i] );
	}

	/*
	** rotate the bounding box
	*/
	VectorCopy( e->angles, angles );
	angles[YAW] = -angles[YAW];
	AngleVectors( angles, vectors[0], vectors[1], vectors[2] );

	for ( i = 0; i < 8; i++ )
	{
		vec3_t tmp;

		VectorCopy( bbox[i], tmp );

		bbox[i][0] = DotProduct( vectors[0], tmp );
		bbox[i][1] = -DotProduct( vectors[1], tmp );
		bbox[i][2] = DotProduct( vectors[2], tmp );

		VectorAdd( e->origin, bbox[i], bbox[i] );
	}
#endif

	// cull
	{
		int p, f, aggregatemask = ~0;

		for ( p = 0; p < 8; p++ )
		{
			int mask = 0;

			for ( f = 0; f < 4; f++ )
			{
				float dp = DotProduct( frustum[f].normal, bbox[p] );

				if ( ( dp - frustum[f].dist ) < 0 )
				{
					mask |= ( 1 << f );
				}
			}

			aggregatemask &= mask;
		}

		if ( aggregatemask )
		{
			return true;
		}

		return false;
	}
}

/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel (entity_t *e)
{
	int			i;
	dmdl_t		*paliashdr;
	float		an;
	vec3_t		bbox[8];
	image_t		*skin;

	if ( !( e->flags & RF_WEAPONMODEL ) )
	{
		if ( R_CullAliasModel( bbox, e ) )
			return;
	}

	if ( e->flags & RF_WEAPONMODEL )
	{
		if ( r_lefthand->value == 2 )
			return;
	}

	paliashdr = (dmdl_t *)currentmodel->extradata;

	//
	// get lighting information
	//
	// PMM - rewrote, reordered to handle new shells & mixing
	// PMM - 3.20 code .. replaced with original way of doing it to keep mod authors happy
	//
	if ( currententity->flags & ( RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE ) )
	{
		VectorClear (shadelight);
		if (currententity->flags & RF_SHELL_HALF_DAM)
		{
				shadelight[0] = 0.56;
				shadelight[1] = 0.59;
				shadelight[2] = 0.45;
		}
		if ( currententity->flags & RF_SHELL_DOUBLE )
		{
			shadelight[0] = 0.9;
			shadelight[1] = 0.7;
		}
		if ( currententity->flags & RF_SHELL_RED )
			shadelight[0] = 1.0;
		if ( currententity->flags & RF_SHELL_GREEN )
			shadelight[1] = 1.0;
		if ( currententity->flags & RF_SHELL_BLUE )
			shadelight[2] = 1.0;
	}
/*
		// PMM -special case for godmode
		if ( (currententity->flags & RF_SHELL_RED) &&
			(currententity->flags & RF_SHELL_BLUE) &&
			(currententity->flags & RF_SHELL_GREEN) )
		{
			for (i=0 ; i<3 ; i++)
				shadelight[i] = 1.0;
		}
		else if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE ) )
		{
			VectorClear (shadelight);

			if ( currententity->flags & RF_SHELL_RED )
			{
				shadelight[0] = 1.0;
				if (currententity->flags & (RF_SHELL_BLUE|RF_SHELL_DOUBLE) )
					shadelight[2] = 1.0;
			}
			else if ( currententity->flags & RF_SHELL_BLUE )
			{
				if ( currententity->flags & RF_SHELL_DOUBLE )
				{
					shadelight[1] = 1.0;
					shadelight[2] = 1.0;
				}
				else
				{
					shadelight[2] = 1.0;
				}
			}
			else if ( currententity->flags & RF_SHELL_DOUBLE )
			{
				shadelight[0] = 0.9;
				shadelight[1] = 0.7;
			}
		}
		else if ( currententity->flags & ( RF_SHELL_HALF_DAM | RF_SHELL_GREEN ) )
		{
			VectorClear (shadelight);
			// PMM - new colors
			if ( currententity->flags & RF_SHELL_HALF_DAM )
			{
				shadelight[0] = 0.56;
				shadelight[1] = 0.59;
				shadelight[2] = 0.45;
			}
			if ( currententity->flags & RF_SHELL_GREEN )
			{
				shadelight[1] = 1.0;
			}
		}
	}
			//PMM - ok, now flatten these down to range from 0 to 1.0.
	//		max_shell_val = max(shadelight[0], max(shadelight[1], shadelight[2]));
	//		if (max_shell_val > 0)
	//		{
	//			for (i=0; i<3; i++)
	//			{
	//				shadelight[i] = shadelight[i] / max_shell_val;
	//			}
	//		}
	// pmm
*/
	else if ( currententity->flags & RF_FULLBRIGHT )
	{
		for (i=0 ; i<3 ; i++)
			shadelight[i] = 1.0;
	}
	else
	{
		R_LightPoint (currententity->origin, shadelight);

		// player lighting hack for communication back to server
		// big hack!
		if ( currententity->flags & RF_WEAPONMODEL )
		{
			// pick the greatest component, which should be the same
			// as the mono value returned by software
			if (shadelight[0] > shadelight[1])
			{
				if (shadelight[0] > shadelight[2])
					r_lightlevel->value = 150*shadelight[0];
				else
					r_lightlevel->value = 150*shadelight[2];
			}
			else
			{
				if (shadelight[1] > shadelight[2])
					r_lightlevel->value = 150*shadelight[1];
				else
					r_lightlevel->value = 150*shadelight[2];
			}

		}
		
		if ( gl_monolightmap->string[0] != '0' )
		{
			float s = shadelight[0];

			if ( s < shadelight[1] )
				s = shadelight[1];
			if ( s < shadelight[2] )
				s = shadelight[2];

			shadelight[0] = s;
			shadelight[1] = s;
			shadelight[2] = s;
		}
	}

	if ( currententity->flags & RF_MINLIGHT )
	{
		for (i=0 ; i<3 ; i++)
			if (shadelight[i] > 0.1)
				break;
		if (i == 3)
		{
			shadelight[0] = 0.1;
			shadelight[1] = 0.1;
			shadelight[2] = 0.1;
		}
	}

	if ( currententity->flags & RF_GLOW )
	{	// bonus items will pulse with time
		float	scale;
		float	min;

		scale = 0.1 * sin(r_newrefdef.time*7);
		for (i=0 ; i<3 ; i++)
		{
			min = shadelight[i] * 0.8;
			shadelight[i] += scale;
			if (shadelight[i] < min)
				shadelight[i] = min;
		}
	}

// =================
// PGM	ir goggles color override
	if ( r_newrefdef.rdflags & RDF_IRGOGGLES && currententity->flags & RF_IR_VISIBLE)
	{
		shadelight[0] = 1.0;
		shadelight[1] = 0.0;
		shadelight[2] = 0.0;
	}
// PGM	
// =================

	shadedots = r_avertexnormal_dots[((int)(currententity->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	
	an = currententity->angles[1]/180*M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize (shadevector);

	//
	// locate the proper data
	//

	c_alias_polys += paliashdr->num_tris;

	//
	// draw all the triangles
	//
	if (currententity->flags & RF_DEPTHHACK) // hack the depth range to prevent view model from poking into walls
		qglDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));

	if ( ( currententity->flags & RF_WEAPONMODEL ) && ( r_lefthand->value == 1.0F ) )
	{
		extern void MYgluPerspective( GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar );

		qglMatrixMode( GL_PROJECTION );
		qglPushMatrix();
		qglLoadIdentity();
		qglScalef( -1, 1, 1 );
	    MYgluPerspective( r_newrefdef.fov_y, ( float ) r_newrefdef.width / r_newrefdef.height,  4,  4096);
		qglMatrixMode( GL_MODELVIEW );

		qglCullFace( GL_BACK );
	}

    qglPushMatrix ();
	e->angles[PITCH] = -e->angles[PITCH];				// sigh.
	e->angles[ROLL] = e->angles[ROLL] * R_RollMult();	// Knightmare- roll is backwards
	R_RotateForEntity (e, true);
	e->angles[PITCH] = -e->angles[PITCH];				// sigh.
	e->angles[ROLL] = e->angles[ROLL] * R_RollMult();	// Knightmare- roll is backwards

	// select skin
	if (currententity->skin)
		skin = currententity->skin;	// custom player skin
	else
	{
		if (currententity->skinnum >= MAX_MD2SKINS)
			skin = currentmodel->skins[0];
		else
		{
			skin = currentmodel->skins[currententity->skinnum];
			if (!skin)
				skin = currentmodel->skins[0];
		}
	}
	if (!skin)
		skin = r_notexture;	// fallback...
	GL_Bind(skin->texnum);

	// setup texture half pixel offset for old models
	if (!skin->is_envmap && gl_fixmodeluvs->value)
	{
		qglMatrixMode(GL_TEXTURE);
		qglPushMatrix();
		qglTranslatef(((1.0f / (float)skin->width)) * -0.5f, ((1.0f / (float)skin->height)) * -0.5f, 0.0f);
		qglMatrixMode(GL_MODELVIEW);
	}

	if (skin->is_envmap)
	{
		qglTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
		qglTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
		qglEnable(GL_TEXTURE_GEN_S);
		qglEnable(GL_TEXTURE_GEN_T);
	}

	// draw it

	qglShadeModel (GL_SMOOTH);

	if (!r_worldmodel->lightdata || !gl_config.mtexcombine || skin->unlit)
	{
		GL_TexEnv(GL_MODULATE);
	}
	else
	{
		GL_TexEnv(GL_COMBINE_EXT);

		qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
		qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
		qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PREVIOUS_EXT);
		if (r_overbrightbits->value)
		{
			qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, r_overbrightbits->value);
		}

		qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_MODULATE);
		qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);
		qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_PREVIOUS_EXT);
		qglTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);
	}

	if (currententity->flags & RF_TRANSLUCENT)
	{
		qglEnable(GL_BLEND);
	}


	if ( (currententity->frame >= paliashdr->num_frames) 
		|| (currententity->frame < 0) )
	{
		ri.Con_Printf (PRINT_ALL, "R_DrawAliasModel %s: no such frame %d\n",
			currentmodel->name, currententity->frame);
		currententity->frame = 0;
		currententity->oldframe = 0;
	}

	if ( (currententity->oldframe >= paliashdr->num_frames)
		|| (currententity->oldframe < 0))
	{
		ri.Con_Printf (PRINT_ALL, "R_DrawAliasModel %s: no such oldframe %d\n",
			currentmodel->name, currententity->oldframe);
		currententity->frame = 0;
		currententity->oldframe = 0;
	}

	if ( !r_lerpmodels->value || currententity->flags & RF_MUZZLEFLASH )
		currententity->backlerp = 0;
	GL_DrawAliasFrameLerp (paliashdr, currententity->backlerp, skin->is_envmap, skin->unlit);

	if (r_worldmodel->lightdata && gl_config.mtexcombine)
	{
		GL_TexEnv(GL_MODULATE);

		qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1);
		qglTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);
	}

	GL_TexEnv( GL_REPLACE );
	qglShadeModel (GL_FLAT);

	qglPopMatrix ();

//#if 1
	if (gl_showbbox->value)	// Knightmare- show bbox option
	{
		qglColor4f (1.0f, 1.0f, 1.0f, 1.0f);
		qglDisable( GL_CULL_FACE );
		qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		qglDisable( GL_TEXTURE_2D );
	/*	qglBegin( GL_TRIANGLE_STRIP );
		for ( i = 0; i < 8; i++ )
		{
			qglVertex3fv( bbox[i] );
		}
		qglEnd();*/
		qglBegin( GL_QUADS );

		qglVertex3fv( bbox[0] );
		qglVertex3fv( bbox[1] );
		qglVertex3fv( bbox[3] );
		qglVertex3fv( bbox[2] );

		qglVertex3fv( bbox[4] );
		qglVertex3fv( bbox[5] );
		qglVertex3fv( bbox[7] );
		qglVertex3fv( bbox[6] );

		qglVertex3fv( bbox[0] );
		qglVertex3fv( bbox[1] );
		qglVertex3fv( bbox[5] );
		qglVertex3fv( bbox[4] );

		qglVertex3fv( bbox[2] );
		qglVertex3fv( bbox[3] );
		qglVertex3fv( bbox[7] );
		qglVertex3fv( bbox[6] );

		qglEnd();

		qglEnable( GL_TEXTURE_2D );
		qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		qglEnable( GL_CULL_FACE );
	}
//#endif

	if ( ( currententity->flags & RF_WEAPONMODEL ) && ( r_lefthand->value == 1.0F ) )
	{
		qglMatrixMode( GL_PROJECTION );
		qglPopMatrix();
		qglMatrixMode( GL_MODELVIEW );
		qglCullFace( GL_FRONT );
	}

	if ( currententity->flags & RF_TRANSLUCENT )
	{
		qglDisable (GL_BLEND);
	}

	if (currententity->flags & RF_DEPTHHACK)
		qglDepthRange (gldepthmin, gldepthmax);

	if (gl_shadows->value && !(currententity->flags & (RF_TRANSLUCENT | RF_WEAPONMODEL)))
	{
	//	qglPushMatrix ();
	//	R_RotateForEntity (e, false);
	//	qglDisable (GL_TEXTURE_2D);
	//	qglEnable (GL_BLEND);
	//	qglColor4f (0, 0, 0, gl_shadowalpha->value); // was 0.5

		GL_DrawAliasShadow (e, paliashdr, currententity->frame );

	//	qglEnable (GL_TEXTURE_2D);
	//	qglDisable (GL_BLEND);
	//	qglPopMatrix ();
	}
	qglColor4f (1,1,1,1);

	// turn off half pixel offset
	if (!skin->is_envmap && gl_fixmodeluvs->value)
	{
		qglMatrixMode(GL_TEXTURE);
		qglPopMatrix();
		qglMatrixMode(GL_MODELVIEW);
	}

	if (skin->is_envmap)
	{
		qglDisable(GL_TEXTURE_GEN_S);
		qglDisable(GL_TEXTURE_GEN_T);
	}
}


