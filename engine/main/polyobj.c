/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.  
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/
/*
 * $Source: f:/miner/source/main/rcs/polyobj.c $
 * $Revision: 2.1 $
 * $Author: john $
 * $Date: 1995/05/26 16:10:37 $
 * 
 * Hacked-in polygon objects
 * 
 * $Log: polyobj.c $
 * Revision 2.1  1995/05/26  16:10:37  john
 * Support for new 4-byte align v8 pof files.
 * 
 * Revision 2.0  1995/02/27  11:32:44  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.64  1995/01/14  19:16:43  john
 * First version of new bitmap paging code.
 * 
 * Revision 1.63  1994/12/14  18:06:54  matt
 * Removed compile warnings
 * 
 * Revision 1.62  1994/12/09  17:54:31  john
 * Made the CFILE's close right after reading in data.
 * 
 * Revision 1.61  1994/12/09  16:13:28  mike
 * speedup pof file reading, but still horribly slow using hog file...problem somewhere else.
 * 
 * Revision 1.60  1994/12/08  17:41:20  yuan
 * Cfiling stuff.
 * 
 * Revision 1.59  1994/11/21  11:02:19  matt
 * Added error checking
 * 
 * Revision 1.58  1994/11/14  11:32:49  matt
 * Allow switching to simpler models even when alt_textures specified
 * 
 * Revision 1.57  1994/11/13  21:15:24  matt
 * Added basic support for more than one level of detail simplification
 * 
 * Revision 1.56  1994/11/11  19:29:25  matt
 * Added code to show low detail polygon models
 * 
 * Revision 1.55  1994/11/10  14:02:57  matt
 * Hacked in support for player ships with different textures
 * 
 * Revision 1.54  1994/11/03  11:01:59  matt
 * Made robot pics lighted
 * 
 * Revision 1.53  1994/11/02  16:18:34  matt
 * Moved draw_model_picture() out of editor
 * 
 * Revision 1.52  1994/10/18  14:38:11  matt
 * Restored assert now that bug is fixed
 * 
 * Revision 1.51  1994/10/17  21:35:03  matt
 * Added support for new Control Center/Main Reactor
 * 
 * Revision 1.50  1994/10/14  17:46:23  yuan
 * Made the soft Int3 only work in net mode.
 * 
 * Revision 1.49  1994/10/14  17:43:47  yuan
 * Added soft int3's instead of Asserts  for some common network bugs.
 * 
 * Revision 1.48  1994/10/14  17:09:04  yuan
 * Made Assert on line 610 be if in an attempt
 * to bypass.
 * 
 * Revision 1.47  1994/09/09  14:23:42  matt
 * Added glow code to polygon models for engine glow
 * 
 * Revision 1.46  1994/08/26  18:03:30  matt
 * Added code to remap polygon model numbers by matching filenames
 * 
 * Revision 1.45  1994/08/26  15:35:58  matt
 * Made eclips usable on more than one object at a time
 * 
 * Revision 1.44  1994/08/25  18:11:58  matt
 * Made player's weapons and flares fire from the positions on the 3d model.
 * Also added support for quad lasers.
 * 
 * Revision 1.43  1994/07/25  00:14:18  matt
 * Made a couple of minor changes for the drivethrough
 * 
 * Revision 1.42  1994/07/25  00:02:41  matt
 * Various changes to accomodate new 3d, which no longer takes point numbers
 * as parms, and now only takes pointers to points.
 * 
 */


#pragma off (unreferenced)
static char rcsid[] = "$Id: polyobj.c 2.1 1995/05/26 16:10:37 john Exp $";
#pragma on (unreferenced)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// ASCENT: io.h (DOS) is removed.

#ifdef DRIVE
#include "drive.h"
#else
#include "inferno.h"
#endif

#include "polyobj.h"

#include "vecmat.h"
#include "3d.h"
#include "error.h"
#include "mono.h"
#include "mem.h"
#include "args.h"

#ifndef DRIVE
#include "texmap.h"
#include "bm.h"
#include "textures.h"
#include "object.h"
#include "lighting.h"
#include "cfile.h"
#include "piggy.h"
#endif

#include "dataio.h"		// ASCENT: little endian field reads
#include "dtufile.h"		// ASCENT: dtu_log()

polymodel Polygon_models[MAX_POLYGON_MODELS];	// = {&bot11,&bot17,&robot_s2,&robot_b2,&bot11,&bot17,&robot_s2,&robot_b2};

int N_polygon_models = 0;

#define MAX_POLYGON_VECS 1000
g3s_point robot_points[MAX_POLYGON_VECS];

#define PM_COMPATIBLE_VERSION 6
#define PM_OBJFILE_VERSION 8

int	Pof_file_end;
int	Pof_addr;

#define	MODEL_BUF_SIZE	32768

// ASCENT: the sizes of a vector and of an angle vector in a POF file
#define VMS_VECTOR_DISK_SIZE	12
#define VMS_ANGVEC_DISK_SIZE	6

void _pof_cfseek(int len,int type)
{
	switch (type) {
		case SEEK_SET:	Pof_addr = len;	break;
		case SEEK_CUR:	Pof_addr += len;	break;
		case SEEK_END:
			Assert(len <= 0);	//	seeking from end, better be moving back.
			Pof_addr = Pof_file_end + len;
			break;
	}

	if (Pof_addr > MODEL_BUF_SIZE)
		Int3();
}

#define pof_cfseek(_buf,_len,_type) _pof_cfseek((_len),(_type))

int pof_read_int(ubyte *bufp)
{
	int i;
	const ubyte *p = &bufp[Pof_addr];

	// ASCENT: was a read through an (int *) cast, which is not aligned
	// and depends on the byte order of the processor.
	i = dio_get_int(&p);
	Pof_addr += 4;
	return i;

//	if (cfread(&i,sizeof(i),1,f) != 1)
//		Error("Unexpected end-of-file while reading object");
//
//	return i;
}

size_t pof_cfread(void *dst, size_t elsize, size_t nelem, ubyte *bufp)
{
	if (Pof_addr + nelem*elsize > Pof_file_end)
		return 0;

	memcpy(dst, &bufp[Pof_addr], elsize*nelem);

	Pof_addr += elsize*nelem;

	if (Pof_addr > MODEL_BUF_SIZE)
		Int3();

	return nelem;
}

// #define new_read_int(i,f) cfread(&(i),sizeof(i),1,(f))
// ASCENT: was "pof_cfread(&(i),sizeof(i),1,(f))", a copy of the 4 bytes into
// the int. Now a little endian read with the same end-of-file check: it
// gives 1 and moves ahead when the 4 bytes are there, else 0.
static int _new_pof_read_int(int *i, ubyte *bufp)
{
	ubyte b[4];
	const ubyte *p = b;

	if (pof_cfread(b,4,1,bufp) != 1)
		return 0;
	*i = dio_get_int(&p);
	return 1;
}
#define new_pof_read_int(i,f) _new_pof_read_int(&(i),(f))

short pof_read_short(ubyte *bufp)
{
	short s;
	const ubyte *p = &bufp[Pof_addr];

	// ASCENT: was a read through a (short *) cast, see pof_read_int()
	s = dio_get_short(&p);
	Pof_addr += 2;
	return s;
//	if (cfread(&s,sizeof(s),1,f) != 1)
//		Error("Unexpected end-of-file while reading object");
//
//	return s;
}

pof_read_string(char *buf,int max, ubyte *bufp)
{
	int	i;

	for (i=0; i<max; i++) {
		if ((*buf++ = bufp[Pof_addr++]) == 0)
			break;
	}

//	while (max-- && (*buf=cfgetc(f)) != 0) buf++;

}

pof_read_vecs(vms_vector *vecs,int n,ubyte *bufp)
{
//	cfread(vecs,sizeof(vms_vector),n,f);

	// ASCENT: was a memcpy() of the vectors. Each vector is 12 bytes in the
	// file (three fix values, little endian).
	{
		const ubyte *p = &bufp[Pof_addr];
		int i;

		for (i=0; i<n; i++)
			dio_get_vector(&p, &vecs[i]);
		Pof_addr += n*VMS_VECTOR_DISK_SIZE;
	}
	
	if (Pof_addr > MODEL_BUF_SIZE)
		Int3();
}

#define ID_OHDR 'RDHO'	//Object header
#define ID_SOBJ 'JBOS'	//Subobject header
#define ID_GUNS 'SNUG'	//List of guns on this object
#define ID_ANIM 'MINA'	//Animation data
#define ID_IDTA 'ATDI'	//Interpreter data
#define ID_TXTR 'RTXT'	//Texture filename list

#ifdef DRIVE
#define robot_info void
#else
vms_angvec anim_angs[N_ANIM_STATES][MAX_SUBMODELS];

//set the animation angles for this robot.  Gun fields of robot info must
//be filled in.
int robot_set_angles(robot_info *r,polymodel *pm,vms_angvec angs[N_ANIM_STATES][MAX_SUBMODELS]);		// ASCENT: "int" added, the type was implicit
#endif

//reads a binary file containing a 3d model
polymodel *read_model_file(polymodel *pm,char *filename,robot_info *r)
{
	CFILE *ifile;
	short version;
	int id,len, next_chunk;
	ubyte	model_buf[MODEL_BUF_SIZE];

	if ((ifile=cfopen(filename,"rb"))==NULL) 
		Error("Can't open file <%s>",filename);

	Assert(ifile->size <= MODEL_BUF_SIZE);

	Pof_addr = 0;
	Pof_file_end = cfread(model_buf, 1, cfilelength(ifile), ifile);
	cfclose(ifile);

	id = pof_read_int(model_buf);

	if (id!='OPSP')
		Error("Bad ID in model file <%s>",filename);

	version = pof_read_short(model_buf);
	
	if (version < PM_COMPATIBLE_VERSION || version > PM_OBJFILE_VERSION)
		Error("Bad version (%d) in model file <%s>",version,filename);

	if ( FindArg( "-bspgen" )) 
		fprintf( stderr, "bspgen -c1" );

	while (new_pof_read_int(id,model_buf) == 1) {

		//id  = pof_read_int(model_buf);
		len = pof_read_int(model_buf);
		next_chunk = Pof_addr + len;
						
		switch (id) {

			case ID_OHDR: {		//Object header
				vms_vector pmmin,pmmax;

				//mprintf(0,"Got chunk OHDR, len=%d\n",len);

				pm->n_models = pof_read_int(model_buf);
				pm->rad = pof_read_int(model_buf);

				Assert(pm->n_models <= MAX_SUBMODELS);

				pof_read_vecs(&pmmin,1,model_buf);
				pof_read_vecs(&pmmax,1,model_buf);

				if ( FindArg( "-bspgen" ))	{
					vms_vector v;
					fix l;
				
					vm_vec_sub(&v, &pmmax, &pmmin );
					l = v.x;
					if ( v.y > l ) l = v.y;					
					if ( v.z > l ) l = v.z;					
													
					fprintf( stderr, " -l%.3f", f2fl(l) );
				}

				break;
			}
			
			case ID_SOBJ: {		//Subobject header
				int n;

				//mprintf(0,"Got chunk SOBJ, len=%d\n",len);

				n = pof_read_short(model_buf);

				Assert(n < MAX_SUBMODELS);

				pm->submodel_parents[n] = pof_read_short(model_buf);

				pof_read_vecs(&pm->submodel_norms[n],1,model_buf);
				pof_read_vecs(&pm->submodel_pnts[n],1,model_buf);
				pof_read_vecs(&pm->submodel_offsets[n],1,model_buf);

				pm->submodel_rads[n] = pof_read_int(model_buf);		//radius

				pm->submodel_ptrs[n] = pof_read_int(model_buf);	//offset

				break;

			}
			
			#ifndef DRIVE
			case ID_GUNS: {		//List of guns on this object

				//mprintf(0,"Got chunk GUNS, len=%d\n",len);

				if (r) {
					int i;
					vms_vector gun_dir;

					r->n_guns = pof_read_int(model_buf);

					Assert(r->n_guns <= MAX_GUNS);

					for (i=0;i<r->n_guns;i++) {
						int id;

						id = pof_read_short(model_buf);
						r->gun_submodels[id] = pof_read_short(model_buf);
						Assert(r->gun_submodels[id] != 0xff);
						pof_read_vecs(&r->gun_points[id],1,model_buf);

						if (version >= 7)
							pof_read_vecs(&gun_dir,1,model_buf);
					}
				}
				else
					pof_cfseek(model_buf,len,SEEK_CUR);

				break;
			}
			
			case ID_ANIM:		//Animation data
				//mprintf(0,"Got chunk ANIM, len=%d\n",len);

				if ( FindArg( "-bspgen" ))
					fprintf( stderr, " -a" );
				
				if (r) {
					int n_frames,f,m;

					n_frames = pof_read_short(model_buf);

					Assert(n_frames == N_ANIM_STATES);

					for (m=0;m<pm->n_models;m++)
						for (f=0;f<n_frames;f++)
							{
								// ASCENT: was a pof_cfread() of the structure. An
								// angle vector is 6 bytes in the file (three shorts).
								ubyte ang_buf[VMS_ANGVEC_DISK_SIZE];
								const ubyte *ang_p = ang_buf;

								if (pof_cfread(ang_buf,1,VMS_ANGVEC_DISK_SIZE,model_buf))
									dio_get_angvec(&ang_p, &anim_angs[f][m]);
							}

					robot_set_angles(r,pm,anim_angs);
				
				}
				else
					pof_cfseek(model_buf,len,SEEK_CUR);

				break;
			#endif
			
			case ID_TXTR: {		//Texture filename list
				int n;
				char name_buf[128];

				//mprintf(0,"Got chunk TXTR, len=%d\n",len);

				n = pof_read_short(model_buf);
				//mprintf(0,"  num textures = %d\n",n);
				while (n--) {
					pof_read_string(name_buf,128,model_buf);
					//mprintf(0,"<%s>\n",name_buf);
				}

				break;
			}
			
			case ID_IDTA:		//Interpreter data
				//mprintf(0,"Got chunk IDTA, len=%d\n",len);

				pm->model_data = malloc(len);
				pm->model_data_size = len;
			
				pof_cfread(pm->model_data,1,len,model_buf);
			
				break;

			default:
				//mprintf(0,"Unknown chunk <%c%c%c%c>, len = %d\n",id,id>>8,id>>16,id>>24,len);
				pof_cfseek(model_buf,len,SEEK_CUR);
				break;

		}
		if ( version >= 8 )		// Version 8 needs 4-byte alignment!!!
			pof_cfseek(model_buf,next_chunk,SEEK_SET);
	}

//	for (i=0;i<pm->n_models;i++)
//		pm->submodel_ptrs[i] += (int) pm->model_data;

	if ( FindArg( "-bspgen" )) {
		char *p = strchr( filename, '.' );
		*p = 0;
		fprintf( stderr, " %s.3ds\n", filename );
		*p = '.';
	}

	return pm;
}

//reads the gun information for a model
//fills in arrays gun_points & gun_dirs, returns the number of guns read
int read_model_guns(char *filename,vms_vector *gun_points, vms_vector *gun_dirs, int *gun_submodels)
{
	CFILE *ifile;
	short version;
	int id,len;
	int n_guns=0;
	ubyte	model_buf[MODEL_BUF_SIZE];

	if ((ifile=cfopen(filename,"rb"))==NULL) 
		Error("Can't open file <%s>",filename);

	Assert(ifile->size <= MODEL_BUF_SIZE);

	Pof_addr = 0;
	Pof_file_end = cfread(model_buf, 1, ifile->size, ifile);
	cfclose(ifile);

	id = pof_read_int(model_buf);

	if (id!='OPSP')
		Error("Bad ID in model file <%s>",filename);

	version = pof_read_short(model_buf);
	
	Assert(version >= 7);		//must be 7 or higher for this data

	if (version < PM_COMPATIBLE_VERSION || version > PM_OBJFILE_VERSION)
		Error("Bad version (%d) in model file <%s>",version,filename);

	while (new_pof_read_int(id,model_buf) == 1) {

		//id  = pof_read_int(model_buf);
		len = pof_read_int(model_buf);

		if (id == ID_GUNS) {		//List of guns on this object

			//mprintf(0,"Got chunk GUNS, len=%d\n",len);

			int i;

			n_guns = pof_read_int(model_buf);

			for (i=0;i<n_guns;i++) {
				int id,sm;

				id = pof_read_short(model_buf);
				sm = pof_read_short(model_buf);
				if (gun_submodels)
					gun_submodels[id] = sm;
				else if (sm!=0)
					Error("Invalid gun submodel in file <%s>",filename);
				pof_read_vecs(&gun_points[id],1,model_buf);

				pof_read_vecs(&gun_dirs[id],1,model_buf);
			}

		}
		else
			pof_cfseek(model_buf,len,SEEK_CUR);

	}

	return n_guns;
}

//free up a model, getting rid of all its memory
free_model(polymodel *po)
{
	// ASCENT: the model data of an EGG model is the start of dtu_block.
	if (po->dtu_block)
		free(po->dtu_block);
	else if (po->model_data)
		free(po->model_data);
	po->dtu_block = NULL;
	po->model_data = NULL;
	if (po->dtu_arrays)
		free(po->dtu_arrays);
	po->dtu_arrays = NULL;
}

grs_bitmap *texture_list[MAX_POLYOBJ_TEXTURES];
bitmap_index texture_list_index[MAX_POLYOBJ_TEXTURES];

int Simple_model_threshhold_scale=5;		//switch when this times radius far away


//draw a polygon model

void draw_polygon_model(vms_vector *pos,vms_matrix *orient,vms_angvec *anim_angles,int model_num,int flags,fix light,fix *glow_values,bitmap_index alt_textures[])
{
	polymodel *po;
	int i;

	Assert(model_num < N_polygon_models);

	po=&Polygon_models[model_num];

	//check if should use simple model
	if (po->simpler_model )					//must have a simpler model
		if (flags==0)							//can't switch if this is debris
			//!!if (!alt_textures) {				//alternate textures might not match
			//alt textures might not match, but in the one case we're using this
			//for on 11/14/94, they do match.  So we leave it in.
			{
				int cnt=1;
				fix depth;
	
				depth = g3_calc_point_depth(pos);		//gets 3d depth

				while (po->simpler_model && depth > cnt++ * Simple_model_threshhold_scale * po->rad)
					po = &Polygon_models[po->simpler_model-1];
			}

	if (alt_textures)
		for (i=0;i<po->n_textures;i++)	{
			texture_list_index[i] = alt_textures[i];
			texture_list[i] = &GameBitmaps[alt_textures[i].index];
		}
	else
		for (i=0;i<po->n_textures;i++)	{
			texture_list_index[i] = ObjBitmaps[ObjBitmapPtrs[po->first_texture+i]];
			texture_list[i] = &GameBitmaps[ObjBitmaps[ObjBitmapPtrs[po->first_texture+i]].index];
		}

	// ASCENT: the table can give a model fewer textures than its data uses
	// (the loader warns about it). The other entries get bitmap 0 (the
	// "bogus" bitmap), so that the 3d code never reads an old pointer.
	for (i=po->n_textures;i<po->dtu_textures_used && i<MAX_POLYOBJ_TEXTURES;i++) {
		texture_list_index[i].index = 0;
		texture_list[i] = &GameBitmaps[0];
	}

#ifdef PIGGY_USE_PAGING
	// Make sure the textures for this object are paged in...
	piggy_page_flushed = 0;
	for (i=0;i<po->n_textures;i++)	
		PIGGY_PAGE_IN( texture_list_index[i] );
	// Hmmm... cache got flushed in the middle of paging all these in,
	// so we need to reread them all in.
	if (piggy_page_flushed)	{
		piggy_page_flushed = 0;
		for (i=0;i<po->n_textures;i++)	
			PIGGY_PAGE_IN( texture_list_index[i] );
	}
	// Make sure that they can all fit in memory.
	Assert( piggy_page_flushed == 0 );
#endif

	g3_start_instance_matrix(pos,orient);

	g3_set_interp_points(robot_points);

	if (flags == 0)		//draw entire object

		g3_draw_polygon_model(po->model_data,texture_list,anim_angles,light,glow_values);

	else {
		int i;
	
		for (i=0;flags;flags>>=1,i++)
			if (flags & 1) {
				vms_vector ofs;

				Assert(i < po->n_models);

				//if submodel, rotate around its center point, not pivot point
	
				vm_vec_avg(&ofs,&po->submodel_mins[i],&po->submodel_maxs[i]);
				vm_vec_negate(&ofs);
				g3_start_instance_matrix(&ofs,NULL);
	
				g3_draw_polygon_model(&po->model_data[po->submodel_ptrs[i]],texture_list,anim_angles,light,glow_values);
	
				g3_done_instance();
			}	
	}

	g3_done_instance();

}

// ASCENT: the function had no return type (int). atexit() needs a function
// that returns void.
void free_polygon_models(void)
{
	int i;

	for (i=0;i<N_polygon_models;i++) {
		free_model(&Polygon_models[i]);
	}

}

polyobj_find_min_max(polymodel *pm)
{
	ushort nverts;
	vms_vector *vp;
	ushort *data,type;
	int m;
	vms_vector *big_mn,*big_mx;
	
	big_mn = &pm->mins;
	big_mx = &pm->maxs;

	for (m=0;m<pm->n_models;m++) {
		vms_vector *mn,*mx,*ofs;

		mn = &pm->submodel_mins[m];
		mx = &pm->submodel_maxs[m];
		ofs= &pm->submodel_offsets[m];

		data = (ushort *)&pm->model_data[pm->submodel_ptrs[m]];
	
		type = *data++;
	
		Assert(type == 7 || type == 1);
	
		nverts = *data++;
	
		if (type==7)
			data+=2;		//skip start & pad
	
		vp = (vms_vector *) data;
	
		*mn = *mx = *vp++; nverts--;

		if (m==0)
			*big_mn = *big_mx = *mn;
	
		while (nverts--) {
			if (vp->x > mx->x) mx->x = vp->x;
			if (vp->y > mx->y) mx->y = vp->y;
			if (vp->z > mx->z) mx->z = vp->z;
	
			if (vp->x < mn->x) mn->x = vp->x;
			if (vp->y < mn->y) mn->y = vp->y;
			if (vp->z < mn->z) mn->z = vp->z;
	
			if (vp->x+ofs->x > big_mx->x) big_mx->x = vp->x+ofs->x;
			if (vp->y+ofs->y > big_mx->y) big_mx->y = vp->y+ofs->y;
			if (vp->z+ofs->z > big_mx->z) big_mx->z = vp->z+ofs->z;
	
			if (vp->x+ofs->x < big_mn->x) big_mn->x = vp->x+ofs->x;
			if (vp->y+ofs->y < big_mn->y) big_mn->y = vp->y+ofs->y;
			if (vp->z+ofs->z < big_mn->z) big_mn->z = vp->z+ofs->z;
	
			vp++;
		}

//		printf("Submodel %d:  (%8x,%8x) (%8x,%8x) (%8x,%8x)\n",m,mn->x,mx->x,mn->y,mx->y,mn->z,mx->z);
	}

//	printf("Whole model: (%8x,%8x) (%8x,%8x) (%8x,%8x)\n",big_mn->x,big_mx->x,big_mn->y,big_mx->y,big_mn->z,big_mx->z);

}

extern short highest_texture_num;	//from the 3d

char Pof_names[MAX_POLYGON_MODELS][13];

//returns the number of this model
#ifndef DRIVE
int load_polygon_model(char *filename,int n_textures,int first_texture,robot_info *r)
#else
int load_polygon_model(char *filename,int n_textures,grs_bitmap ***textures)
#endif
{
	#ifdef DRIVE
	#define r NULL
	#endif

	// ASCENT: Descent 1 read the POF file here. In Descent to Undermountain
	// the models are in the EGG archive, and piggy.c has already loaded them
	// (polyobj_load_dtu_model below). DTUGAME.EXE does the same: it looks for
	// the name among the loaded models (code at 0x72DD4) and returns that
	// number. A model that is not in the archive gives -1 here. The original
	// then read NAME.POF from the disk, and no such file ships.
	//
	// DTUGAME.EXE keeps the first_texture of the EGG header for a model
	// that it finds. This port does not: the header value depends on the
	// exact slot numbers of the original table parser, which are verified
	// only in part (docs/formats/tbl.md). The first table line that names a
	// model gives it the slots of its texture list. A later line with the
	// same model name gets the same model, with the textures of the first
	// line, as in the original.
	int mn;
	polymodel *pm;

	Assert(n_textures < MAX_POLYOBJ_TEXTURES);

	mn = polyobj_find_model(filename);
	if (mn < 0) {
		dtu_log( "Model <%s> is not in the EGG archive\n", filename );
		return -1;
	}

	pm = &Polygon_models[mn];

	if (!pm->dtu_bound) {
		if (n_textures != pm->dtu_n_textures_file || n_textures < pm->dtu_textures_used)
			dtu_log( "Model <%s>: the table gives %d textures, the header has %d, the data uses %d\n",
				filename, n_textures, pm->dtu_n_textures_file, pm->dtu_textures_used );
		if (n_textures >= MAX_POLYOBJ_TEXTURES)
			n_textures = MAX_POLYOBJ_TEXTURES-1;
		pm->n_textures = n_textures;
		pm->first_texture = first_texture;
		pm->simpler_model = 0;
		pm->dtu_bound = 1;
	}

	// ASCENT: Descent 1 filled the gun points and the joint positions of a
	// robot from the POF file. Where the EGG model keeps them is not known.
	if (r)
		r->n_guns = 0;

	return mn;

}

// ASCENT: the functions below are new. They load the polygon models of an
// EGG archive. See docs/formats/egg.md for the layout.

// Walk the opcodes of the interpreter data without a change to the data.
// Returns 0 if an opcode is unknown or the data ends too early. Sets
// *highest_tex to the highest texture number of a textured polygon.
// The record sizes are those of init_loop() in 3d/interp.c.
static int dtu_check_model_data(const ubyte *data, int size, int ofs, int depth, int *highest_tex, int *n_ops)
{
	int op, n;

	if (depth > 64)
		return 0;

	for (;;) {
		if (ofs < 0 || ofs+2 > size)
			return 0;
		if (++(*n_ops) > 200000)
			return 0;

		op = data[ofs] | (data[ofs+1]<<8);

		#define DTU_W(o) (((ofs)+(o)+2 <= size) ? (data[ofs+(o)] | (data[ofs+(o)+1]<<8)) : 0)

		switch (op) {
		case 0:				//OP_EOF
			return 1;
		case 1:				//OP_DEFPOINTS
			n = DTU_W(2);
			ofs += 4 + n*12;
			break;
		case 2:				//OP_FLATPOLY
			n = DTU_W(2);
			ofs += 30 + ((n*2)|2);		//pointnum_list_size
			break;
		case 3:				//OP_TMAPPOLY
			n = (short) DTU_W(28);
			if (n > *highest_tex)
				*highest_tex = n;
			n = DTU_W(2);
			ofs += 30 + ((n*2)|2) + n*12;
			break;
		case 4:				//OP_SORTNORM
			if (!dtu_check_model_data(data,size,ofs+DTU_W(28),depth+1,highest_tex,n_ops))
				return 0;
			if (!dtu_check_model_data(data,size,ofs+DTU_W(30),depth+1,highest_tex,n_ops))
				return 0;
			ofs += 32;
			break;
		case 5:				//OP_RODBM
			ofs += 36;
			break;
		case 6:				//OP_SUBCALL
			if (!dtu_check_model_data(data,size,ofs+DTU_W(16),depth+1,highest_tex,n_ops))
				return 0;
			ofs += 20;
			break;
		case 7:				//OP_DEFP_START
			n = DTU_W(2);
			ofs += 8 + n*12;
			break;
		case 8:				//OP_GLOW
			ofs += 4;
			break;
		default:
			return 0;
		}

		#undef DTU_W
	}
}

int polyobj_find_model(const char *name)
{
	int i;

	for (i=0;i<N_polygon_models;i++)
		if (!stricmp(Pof_names[i],name))
			return i;
	return -1;
}

// Decode the arrays 10 to 19 of the data block and the rest of the header
// into the fields of the model (docs/re/models.md, docs/formats/egg.md).
// Returns 0 if there is no memory.
static int dtu_decode_model_arrays(polymodel *pm)
{
	int n = pm->n_models, F = pm->n_frames, flags = pm->dtu_flags;
	int n_vec, n_fix, n_ang, n_byte, i, total;
	ubyte *mem;
	const ubyte *p;
	vms_vector *vp;
	fix *fp;

	#define SECTION(s) (pm->dtu_block + pm->dtu_section_ofs[s])

	n_vec = 2*(F+1) + 7*F + (F+1) + 3*F;
	n_fix = 2*(F+1) + 4*F;
	n_ang = F*n;
	n_byte = n + n*n;
	total = n_vec*(int)sizeof(vms_vector) + n_fix*(int)sizeof(fix) + n_ang*(int)sizeof(vms_angvec) + n_byte + 16;

	mem = (ubyte *) malloc(total);
	if (mem == NULL)
		return 0;
	memset(mem,0,total);
	pm->dtu_arrays = mem;

	vp = (vms_vector *) mem;
	fp = (fix *) (vp + n_vec);

	// arrays 10 and 11: the body box
	pm->box1_min = vp; vp += F+1;
	pm->box1_max = vp; vp += F+1;
	p = SECTION(10);
	for (i=0;i<F+1;i++) dio_get_vector(&p,&pm->box1_min[i]);
	p = SECTION(11);
	for (i=0;i<F+1;i++) dio_get_vector(&p,&pm->box1_max[i]);

	if (F > 0) {
		vms_vector **list[7] = { &pm->frame_eye, &pm->box2_min, &pm->box2_max, &pm->box4_min, &pm->box4_max, &pm->box8_min, &pm->box8_max };
		int k;

		// array 12: the joint angles
		pm->anim_angles = (vms_angvec *) (fp + n_fix);
		p = SECTION(12);
		for (i=0;i<n_ang;i++) {
			pm->anim_angles[i].p = dio_get_fixang(&p);
			pm->anim_angles[i].b = dio_get_fixang(&p);
			pm->anim_angles[i].h = dio_get_fixang(&p);
		}

		// array 13: seven arrays of one vector per frame
		p = SECTION(13);
		for (k=0;k<7;k++) {
			*list[k] = vp; vp += F;
			for (i=0;i<F;i++) dio_get_vector(&p,&(*list[k])[i]);
		}
	}

	if (flags & DTU_BOX_BODY) {
		p = SECTION(14);
		pm->body_center = vp; vp += F+1;
		pm->body_half_height = fp; fp += F+1;
		pm->body_radius = fp; fp += F+1;
		for (i=0;i<F+1;i++) dio_get_vector(&p,&pm->body_center[i]);
		for (i=0;i<F+1;i++) pm->body_half_height[i] = dio_get_fix(&p);
		for (i=0;i<F+1;i++) pm->body_radius[i] = dio_get_fix(&p);
	}
	if ((flags & DTU_BOX_TORSO) && F > 0) {
		p = SECTION(15);
		pm->torso_center = vp; vp += F;
		pm->torso_half_height = fp; fp += F;
		pm->torso_radius = fp; fp += F;
		for (i=0;i<F;i++) dio_get_vector(&p,&pm->torso_center[i]);
		for (i=0;i<F;i++) pm->torso_half_height[i] = dio_get_fix(&p);
		for (i=0;i<F;i++) pm->torso_radius[i] = dio_get_fix(&p);
	}
	if ((flags & DTU_BOX_WEAPON) && F > 0) {
		p = SECTION(16);
		pm->weapon_center = vp; vp += F;
		pm->weapon_radius = fp; fp += F;
		for (i=0;i<F;i++) dio_get_vector(&p,&pm->weapon_center[i]);
		for (i=0;i<F;i++) pm->weapon_radius[i] = dio_get_fix(&p);
	}
	if ((flags & DTU_BOX_WEAPON2) && F > 0) {
		p = SECTION(17);
		pm->weapon2_center = vp; vp += F;
		pm->weapon2_radius = fp; fp += F;
		for (i=0;i<F;i++) dio_get_vector(&p,&pm->weapon2_center[i]);
		for (i=0;i<F;i++) pm->weapon2_radius[i] = dio_get_fix(&p);
	}

	// arrays 18 and 19. The rows of array 18 have n bytes (the block has
	// n more bytes after the last row).
	pm->submodel_children = mem + total - 16 - n_byte;
	pm->submodel_flags = pm->submodel_children + n*n;
	memcpy(pm->submodel_children,SECTION(18),n*n);
	memcpy(pm->submodel_flags,SECTION(19),n);
	// A model with one submodel has a row that is not initialized.
	if (n == 1)
		pm->submodel_children[0] = 0xff;

	#undef SECTION

	// the header
	pm->box_flags = flags;
	p = pm->dtu_header + 0x4A;
	pm->gun_submodels[0] = dio_get_ubyte(&p);
	pm->gun_submodels[1] = dio_get_ubyte(&p);
	pm->n_guns = dio_get_short(&p);
	dio_get_vector(&p,&pm->gun_points[0]);
	dio_get_vector(&p,&pm->gun_points[1]);
	for (i=0;i<4;i++) pm->fire_frames[i] = dio_get_short(&p);
	for (i=0;i<8;i++) pm->speeds[i] = dio_get_fix(&p);		// the loader of the game copies 8 values (0x70F57)
	for (i=0;i<19;i++) pm->state_times[i] = dio_get_fix(&p);

	return 1;
}

void polyobj_get_body_center(int model_num,int frame,vms_vector *center)
{
	polymodel *po;

	vm_vec_zero(center);
	if (model_num < 0 || model_num >= N_polygon_models)
		return;
	po = &Polygon_models[model_num];
	if (po->body_center == NULL)
		return;
	if (frame < -1 || frame >= po->n_frames)
		frame = -1;
	*center = po->body_center[frame+1];
}

// ASCENT: the box of the drawn points of a model (see polyobj.h). The
// matrix m is held as its columns (rvec, uvec, fvec): m p is
// p.x*rvec + p.y*uvec + p.z*fvec, as g3_start_instance_angles() uses it.
static void pts_mat_vec(vms_vector *dst,const vms_matrix *m,const vms_vector *p)
{
	dst->x = fixmul(p->x,m->rvec.x) + fixmul(p->y,m->uvec.x) + fixmul(p->z,m->fvec.x);
	dst->y = fixmul(p->x,m->rvec.y) + fixmul(p->y,m->uvec.y) + fixmul(p->z,m->fvec.y);
	dst->z = fixmul(p->x,m->rvec.z) + fixmul(p->y,m->uvec.z) + fixmul(p->z,m->fvec.z);
}

#define PTS_LE16(p)	((int)((p)[0] | ((p)[1] << 8)))
#define PTS_LE32(p)	((int)((uint)(p)[0] | ((uint)(p)[1] << 8) | ((uint)(p)[2] << 16) | ((uint)(p)[3] << 24)))

typedef struct {
	const polymodel *po;
	vms_matrix m;
	vms_vector o;
	vms_vector mn, mx;
	int n;
} pts_box_state;

static void pts_box_point(pts_box_state *st,const ubyte *p)
{
	vms_vector v, w;

	v.x = (fix)PTS_LE32(p);
	v.y = (fix)PTS_LE32(p+4);
	v.z = (fix)PTS_LE32(p+8);
	pts_mat_vec(&w,&st->m,&v);
	vm_vec_add2(&w,&st->o);
	if (st->n == 0)
		st->mn = st->mx = w;
	else {
		if (w.x < st->mn.x) st->mn.x = w.x;
		if (w.y < st->mn.y) st->mn.y = w.y;
		if (w.z < st->mn.z) st->mn.z = w.z;
		if (w.x > st->mx.x) st->mx.x = w.x;
		if (w.y > st->mx.y) st->mx.y = w.y;
		if (w.z > st->mx.z) st->mx.z = w.z;
	}
	st->n++;
}

// The points of the opcode stream at ofs (the opcodes of interp.c).
// Returns 0 if the stream leaves the model data.
static int pts_box_walk(pts_box_state *st,int ofs,int depth)
{
	const ubyte *d = st->po->model_data;
	int size = st->po->model_data_size, i;

	if (depth > 32)
		return 0;
	while (ofs >= 0 && ofs + 4 <= size) {
		int op = PTS_LE16(d+ofs), nv;

		switch (op) {
			case 0:		// OP_EOF
				return 1;
			case 1:		// OP_DEFPOINTS
				nv = PTS_LE16(d+ofs+2);
				if (ofs + 4 + 12*nv > size)
					return 0;
				for (i=0;i<nv;i++)
					pts_box_point(st,d+ofs+4+12*i);
				ofs += 4 + 12*nv;
				break;
			case 7:		// OP_DEFP_START
				nv = PTS_LE16(d+ofs+2);
				if (ofs + 8 + 12*nv > size)
					return 0;
				for (i=0;i<nv;i++)
					pts_box_point(st,d+ofs+8+12*i);
				ofs += 8 + 12*nv;
				break;
			case 2:		// OP_FLATPOLY
				nv = PTS_LE16(d+ofs+2);
				ofs += 30 + ((nv & ~1) + 1) * 2;
				break;
			case 3:		// OP_TMAPPOLY
				nv = PTS_LE16(d+ofs+2);
				ofs += 30 + ((nv & ~1) + 1) * 2 + 12*nv;
				break;
			case 4:		// OP_SORTNORM: both sides
				if (ofs + 32 > size)
					return 0;
				if (!pts_box_walk(st,ofs + PTS_LE16(d+ofs+28),depth+1) ||
					!pts_box_walk(st,ofs + PTS_LE16(d+ofs+30),depth+1))
					return 0;
				ofs += 32;
				break;
			case 5:		// OP_RODBM
				ofs += 36;
				break;
			case 6:		// OP_SUBCALL (not in the models of the target game)
				ofs += 20;
				break;
			case 8:		// OP_GLOW
				ofs += 4;
				break;
			default:
				return 0;
		}
	}
	return 0;
}

static int pts_box_submodel(pts_box_state *st,int s,const vms_angvec *angles,const vms_matrix *pm,const vms_vector *po_,int depth)
{
	const polymodel *po = st->po;
	vms_matrix lm, m;
	vms_vector o;
	const ubyte *row;
	int i;

	if (depth > MAX_POLYMODEL_SUBMODELS)
		return 0;
	if (angles)
		vm_angles_2_matrix(&lm,(vms_angvec *)&angles[s]);
	else
		lm = vmd_identity_matrix;
	// m = pm * lm (the columns of lm through pm), o = po + pm * offset
	pts_mat_vec(&m.rvec,pm,&lm.rvec);
	pts_mat_vec(&m.uvec,pm,&lm.uvec);
	pts_mat_vec(&m.fvec,pm,&lm.fvec);
	pts_mat_vec(&o,pm,&po->submodel_offsets[s]);
	vm_vec_add2(&o,(vms_vector *)po_);

	st->m = m;
	st->o = o;
	if (!pts_box_walk(st,po->submodel_ptrs[s],0))
		return 0;
	if (po->submodel_children == NULL)
		return 1;
	row = po->submodel_children + s*po->n_models;
	for (i=0;i<po->n_models && row[i] != 0xff;i++)
		if (row[i] < po->n_models && !pts_box_submodel(st,row[i],angles,&m,&o,depth+1))
			return 0;
	return 1;
}

int polyobj_frame_points_box(int model_num,int frame,vms_vector *mn,vms_vector *mx)
{
	pts_box_state st;
	const vms_angvec *angles;
	vms_matrix id;
	vms_vector zero = ZERO_VECTOR, c;

	if (model_num < 0 || model_num >= N_polygon_models)
		return 0;
	st.po = &Polygon_models[model_num];
	if (st.po->model_data == NULL || st.po->n_models <= 0)
		return 0;
	if (frame > -1 && (st.po->anim_angles == NULL || frame >= st.po->n_frames))
		frame = -1;
	angles = (frame == -1) ? NULL : st.po->anim_angles + frame*st.po->n_models;
	st.n = 0;
	id = vmd_identity_matrix;
	if (st.po->n_models > 1) {
		if (!pts_box_submodel(&st,0,angles,&id,&zero,0))
			return 0;
	}
	else {
		// draw_polygon_model_frame() draws one submodel with no instance
		st.m = id;
		st.o = zero;
		if (!pts_box_walk(&st,0,0))
			return 0;
	}
	if (st.n == 0)
		return 0;
	polyobj_get_body_center(model_num,frame,&c);
	vm_vec_sub(mn,&st.mn,&c);
	vm_vec_sub(mx,&st.mx,&c);
	return 1;
}

// The sorted draw of a model with more than one submodel (0x66E13 in
// DTUGAME.EXE, newmodel.c). The model data has no SUBCALL opcode. Pass 1
// walks the tree of the submodels and keeps the instance state and the
// depth of each joint. Pass 2 draws the submodels, far ones first.
static g3s_instance_state Sorted_saved[MAX_POLYMODEL_SUBMODELS];
static fix Sorted_depth[MAX_POLYMODEL_SUBMODELS];
static ubyte Sorted_visited[MAX_POLYMODEL_SUBMODELS];

static void sorted_draw_walk(polymodel *po,int s,vms_angvec *angles,int depth)
{
	vms_vector zero = ZERO_VECTOR;
	g3s_point origin;
	ubyte *row;
	int i;

	// A child list of the data never has a loop. The test protects
	// against a damaged file.
	if (depth > MAX_POLYMODEL_SUBMODELS || Sorted_visited[s])
		return;
	Sorted_visited[s] = 1;

	g3_start_instance_angles(&po->submodel_offsets[s],angles ? &angles[s] : NULL);
	g3_get_instance_state(&Sorted_saved[s]);
	g3_rotate_point(&origin,&zero);
	Sorted_depth[s] = origin.p3_vec.z;

	row = po->submodel_children + s*po->n_models;
	for (i=0;i<po->n_models && row[i] != 0xff;i++)
		if (row[i] < po->n_models)
			sorted_draw_walk(po,row[i],angles,depth+1);

	g3_done_instance();
}

static int sorted_draw_compare(const void *a,const void *b)
{
	fix za = Sorted_depth[*(const ubyte *)a], zb = Sorted_depth[*(const ubyte *)b];

	// The largest z first. The order of two equal values is the order of
	// the submodel numbers (the original leaves it to its qsort).
	if (za != zb)
		return (za > zb) ? -1 : 1;
	return (int)*(const ubyte *)a - (int)*(const ubyte *)b;
}

static void draw_model_sorted(polymodel *po,vms_angvec *angles,fix light,fix *glow_values,int show_hidden)
{
	ubyte order[MAX_POLYMODEL_SUBMODELS];
	g3s_instance_state save;
	int i, n = po->n_models;

	memset(Sorted_visited,0,sizeof(Sorted_visited));
	sorted_draw_walk(po,0,angles,0);

	for (i=0;i<n;i++)
		order[i] = i;
	qsort(order,n,1,sorted_draw_compare);

	g3_get_instance_state(&save);
	for (i=0;i<n;i++) {
		int s = order[i];

		if (!Sorted_visited[s])		// not in the tree
			continue;
		if ((po->submodel_flags[s] & DTU_SUBMODEL_F_HIDDEN) && !show_hidden)
			continue;
		g3_set_instance_state(&Sorted_saved[s]);
		g3_draw_polygon_model(&po->model_data[po->submodel_ptrs[s]],texture_list,angles,light,glow_values);
	}
	g3_set_instance_state(&save);
}

void draw_polygon_model_frame(vms_vector *pos,vms_matrix *orient,int model_num,int frame,int show_hidden,fix light,fix *glow_values,bitmap_index alt_textures[])
{
	draw_polygon_model_frame_lerp(pos,orient,model_num,frame,-1,0,show_hidden,light,glow_values,alt_textures);
}

// ASCENT: the angle frac of the way from a to b, the short way around
static fixang angle_lerp(fixang a,fixang b,fix frac)
{
	fixang d = (fixang) (b - a);		// wraps to -0x8000 .. 0x7FFF

	return (fixang) (a + fixmul(d,frac));
}

void draw_polygon_model_frame_lerp(vms_vector *pos,vms_matrix *orient,int model_num,int frame,int next,fix frac,int show_hidden,fix light,fix *glow_values,bitmap_index alt_textures[])
{
	static vms_angvec lerp_angles[MAX_POLYMODEL_SUBMODELS];
	polymodel *po;
	vms_angvec *angles;
	vms_vector draw_pos;
	int i, im_save;

	if (model_num < 0 || model_num >= N_polygon_models) {
		mprintf((1, "Trying to draw bogus poly\n"));
		return;
	}

	po=&Polygon_models[model_num];

	if (po->dtu_block == NULL || po->model_data == NULL)
		return;

	if (frame > -1 && (po->anim_angles == NULL || frame >= po->n_frames))
		frame = -1;
	if (frame == -1 || next < 0 || next >= po->n_frames || next == frame || frac <= 0 || po->n_models > MAX_POLYMODEL_SUBMODELS)
		next = -1;
	if (frac > F1_0)
		frac = F1_0;

	if (alt_textures)
		for (i=0;i<po->n_textures;i++)	{
			texture_list_index[i] = alt_textures[i];
			texture_list[i] = &GameBitmaps[alt_textures[i].index];
		}
	else
		for (i=0;i<po->n_textures;i++)	{
			texture_list_index[i] = ObjBitmaps[ObjBitmapPtrs[po->first_texture+i]];
			texture_list[i] = &GameBitmaps[ObjBitmaps[ObjBitmapPtrs[po->first_texture+i]].index];
		}

	// See draw_polygon_model()
	for (i=po->n_textures;i<po->dtu_textures_used && i<MAX_POLYOBJ_TEXTURES;i++) {
		texture_list_index[i].index = 0;
		texture_list[i] = &GameBitmaps[0];
	}

#ifdef PIGGY_USE_PAGING
	for (i=0;i<po->n_textures;i++)
		PIGGY_PAGE_IN( texture_list_index[i] );
#endif

	// The position of an object is the center of the body box of its
	// current frame (0x729C1 to 0x72A0A).
	draw_pos = *pos;
	if (po->body_center) {
		vms_matrix m;
		vms_vector ofs;

		vms_vector c = po->body_center[frame+1];

		vm_copy_transpose_matrix(&m,orient);
		if (next != -1) {		// ASCENT: animation interpolation
			vms_vector *n = &po->body_center[next+1];
			c.x += fixmul(n->x - c.x,frac);
			c.y += fixmul(n->y - c.y,frac);
			c.z += fixmul(n->z - c.z,frac);
		}
		vm_vec_rotate(&ofs,&c,&m);
		vm_vec_sub2(&draw_pos,&ofs);
	}

	g3_start_instance_matrix(&draw_pos,orient);

	g3_set_interp_points(robot_points);

	angles = (frame == -1) ? NULL : po->anim_angles + frame*po->n_models;

	// ASCENT: animation interpolation: each joint angle between the two
	// frames, the short way around.
	if (next != -1) {
		vms_angvec *a = po->anim_angles + frame*po->n_models;
		vms_angvec *b = po->anim_angles + next*po->n_models;

		for (i=0;i<po->n_models;i++) {
			lerp_angles[i].p = angle_lerp(a[i].p,b[i].p,frac);
			lerp_angles[i].b = angle_lerp(a[i].b,b[i].b,frac);
			lerp_angles[i].h = angle_lerp(a[i].h,b[i].h,frac);
		}
		angles = lerp_angles;
	}

	// ASCENT: draw_polygon_object (0x675D4) sets Interpolation_method to 1
	// (the linear texture mapper) for each object but the viewer, because
	// Linear_tmap_polygon_objects (0x13D3E3) is 1 and nothing changes it.
	// The linear mapper bends the textures of large polygons near the eye
	// (beds, tables). The port draws all models with the perspective
	// mapper. The GPU renderer is always perspective correct.
	im_save = Interpolation_method;
	Interpolation_method = 2;

	if (po->n_models > 1)
		draw_model_sorted(po,angles,light,glow_values,show_hidden);
	else
		g3_draw_polygon_model(po->model_data,texture_list,angles,light,glow_values);

	Interpolation_method = im_save;

	g3_done_instance();
}

// ASCENT: the frame tracker of the animation interpolation, see polyobj.h
fix polyobj_anim_track_update(polyobj_anim_track *t,int key,int state,int frame,int first,int last,fix est,int def_dir,fix now,int *next)
{
	int dir, d;
	fix frac;

	*next = -1;
	if (t->key != key || t->state != state || now < t->t_frame) {
		t->key = key;
		t->state = (short) state;
		t->frame = (short) frame;
		t->dir = 0;
		t->n_adv = 0;
		t->t_frame = now;
		t->dur = est;
	}
	else if (frame != t->frame) {
		d = frame - t->frame;
		if (d >= -2 && d <= 2 && (t->dir == 0 || (d > 0) == (t->dir > 0))) {
			t->dir = d > 0 ? 1 : -1;
			if (t->n_adv == 0) {
				// the first change: the time of a frame is known from the next one
				t->n_adv = 1;
				t->t_first = now;
			}
			else {
				t->n_adv += d > 0 ? d : -d;
				t->dur = (now - t->t_first) / (t->n_adv - 1);
			}
		}
		else {
			// a jump inside the state: start again
			t->dir = 0;
			t->n_adv = 0;
			t->dur = est;
		}
		t->frame = (short) frame;
		t->t_frame = now;
	}

	dir = t->dir ? t->dir : (def_dir < 0 ? -1 : 1);
	if (frame < first || frame >= last || frame + dir < first || frame + dir >= last || t->dur <= 0)
		return 0;
	frac = fixdiv(now - t->t_frame,t->dur);
	if (frac <= 0)
		return 0;
	if (frac > F1_0)
		frac = F1_0;
	*next = frame + dir;
	return frac;
}

int polyobj_load_dtu_model(const ubyte *header, const ubyte *block, int block_avail)
{
	polymodel *pm;
	const ubyte *p;
	int n, F, flags, i, ofs, highest, n_ops;
	int size[DTU_MODEL_SECTIONS];

	if (N_polygon_models >= MAX_POLYGON_MODELS) {
		dtu_log( "Too many polygon models (%d)\n", MAX_POLYGON_MODELS );
		return -1;
	}

	pm = &Polygon_models[N_polygon_models];
	memset(pm,0,sizeof(*pm));

	p = header;
	pm->n_models = dio_get_int(&p);
	pm->model_data_size = dio_get_int(&p);
	dio_get_vector(&p,&pm->mins);
	dio_get_vector(&p,&pm->maxs);
	pm->rad = dio_get_fix(&p);
	pm->dtu_n_textures_file = (ubyte) dio_get_int(&p);
	pm->dtu_first_texture_file = dio_get_int(&p);
	p += 8;									// 0x2C, 0x30: -1 in all files
	pm->n_frames = dio_get_int(&p);
	p += 4;									// 0x38: offset, the caller used it
	pm->dtu_flags = dio_get_ubyte(&p);

	n = pm->n_models;
	F = pm->n_frames;
	flags = pm->dtu_flags;

	if (n < 1 || n > MAX_POLYMODEL_SUBMODELS || F < 0 || F > 100000 || pm->model_data_size < 2)
		return -1;

	// The sizes of the arrays of the data block, in file order.
	memset(size,0,sizeof(size));
	size[1] = pm->model_data_size;
	size[2] = 4*n;
	size[3] = size[4] = size[5] = 12*n;
	size[6] = 4*n;
	size[7] = n;
	size[8] = size[9] = 12*n;
	size[10] = size[11] = 12*(F+1);
	if (F > 0) {
		size[12] = 6*n*F;
		size[13] = 7*12*F;
	}
	if (flags & 1) size[14] = 20*(F+1);
	if (flags & 8) size[15] = 20*F;
	if (flags & 2) size[16] = 16*F;
	if (flags & 4) size[17] = 16*F;
	size[18] = n*(n+1);
	size[19] = n;

	ofs = 0;
	for (i=1;i<DTU_MODEL_SECTIONS;i++) {
		pm->dtu_section_size[i] = size[i];
		pm->dtu_section_ofs[i] = size[i] ? ofs : -1;
		ofs += size[i];
		if (ofs > block_avail || ofs < 0)
			return -1;
	}
	pm->dtu_section_ofs[0] = -1;

	pm->dtu_block_size = ofs;
	pm->dtu_block = (ubyte *) malloc(ofs);
	if (pm->dtu_block == NULL)
		return -1;
	memcpy(pm->dtu_block,block,ofs);
	memcpy(pm->dtu_header,header,DTU_MODEL_HEADER_SIZE);
	pm->model_data = pm->dtu_block;

	p = pm->dtu_block + pm->dtu_section_ofs[2];
	for (i=0;i<n;i++) pm->submodel_ptrs[i] = dio_get_int(&p);
	for (i=0;i<n;i++) dio_get_vector(&p,&pm->submodel_offsets[i]);
	for (i=0;i<n;i++) dio_get_vector(&p,&pm->submodel_norms[i]);
	for (i=0;i<n;i++) dio_get_vector(&p,&pm->submodel_pnts[i]);
	for (i=0;i<n;i++) pm->submodel_rads[i] = dio_get_fix(&p);
	for (i=0;i<n;i++) pm->submodel_parents[i] = dio_get_ubyte(&p);
	for (i=0;i<n;i++) dio_get_vector(&p,&pm->submodel_mins[i]);
	for (i=0;i<n;i++) dio_get_vector(&p,&pm->submodel_maxs[i]);

	// Check the opcode stream. The data is not changed: g3_init_polygon_model
	// is not called, because the flat polygon colors in the archive are
	// already palette indexes. (Guess from Descent 1, which wrote its models
	// to the PIG file after that function. Not verified in DTUGAME.EXE.)
	highest = -1;
	n_ops = 0;
	if (!dtu_check_model_data(pm->model_data,pm->model_data_size,0,0,&highest,&n_ops) || highest >= MAX_POLYOBJ_TEXTURES) {
		free(pm->dtu_block);
		pm->dtu_block = NULL;
		pm->model_data = NULL;
		return -1;
	}
	for (i=0;i<n;i++)
		if (pm->submodel_ptrs[i] < 0 || pm->submodel_ptrs[i] >= pm->model_data_size) {
			free(pm->dtu_block);
			pm->dtu_block = NULL;
			pm->model_data = NULL;
			return -1;
		}
	pm->dtu_textures_used = highest+1;

	if (!dtu_decode_model_arrays(pm)) {
		free(pm->dtu_block);
		pm->dtu_block = NULL;
		pm->model_data = NULL;
		return -1;
	}

	// Until a table line names the model, it draws with the texture slots
	// that follow slot 0. load_polygon_model() sets the real values.
	pm->n_textures = pm->dtu_n_textures_file;
	if (pm->n_textures >= MAX_POLYOBJ_TEXTURES)
		pm->n_textures = MAX_POLYOBJ_TEXTURES-1;
	pm->first_texture = 0;
	pm->simpler_model = 0;
	pm->dtu_bound = 0;

	memcpy(Pof_names[N_polygon_models],header+0x3D,12);
	Pof_names[N_polygon_models][12] = 0;

	N_polygon_models++;

	return N_polygon_models-1;
}

void polyobj_free_all_models(void)
{
	int i;

	for (i=0;i<N_polygon_models;i++)
		free_model(&Polygon_models[i]);
	N_polygon_models = 0;
}

const ubyte *polyobj_get_frame_angles(int model_num, int frame)
{
	polymodel *pm;

	if (model_num < 0 || model_num >= N_polygon_models)
		return NULL;
	pm = &Polygon_models[model_num];
	if (pm->dtu_block == NULL || frame < 0 || frame >= pm->n_frames || pm->dtu_section_ofs[12] < 0)
		return NULL;
	return pm->dtu_block + pm->dtu_section_ofs[12] + frame*6*pm->n_models;
}


init_polygon_models()
{
	N_polygon_models = 0;

	atexit(free_polygon_models);

}

//compare against this size when figuring how far to place eye for picture
#define BASE_MODEL_SIZE 0x28000

#define DEFAULT_VIEW_DIST 0x60000

//draws the given model in the current canvas.  The distance is set to
//more-or-less fill the canvas.  Note that this routine actually renders
//into an off-screen canvas that it creates, then copies to the current
//canvas.
draw_model_picture(int mn,vms_angvec *orient_angles)
{
	vms_vector	temp_pos=ZERO_VECTOR;
	vms_matrix	temp_orient = IDENTITY_MATRIX;
	grs_canvas	*save_canv = grd_curcanv,*temp_canv;
	//int			save_light;

	Assert(mn>=0 && mn<N_polygon_models);

	temp_canv = gr_create_canvas(save_canv->cv_bitmap.bm_w,save_canv->cv_bitmap.bm_h);
	gr_set_current_canvas(temp_canv);
	gr_clear_canvas( BM_XRGB(0,0,0) );

	g3_start_frame();
	g3_set_view_matrix(&temp_pos,&temp_orient,0x9000);

	if (Polygon_models[mn].rad != 0)
		temp_pos.z = fixmuldiv(DEFAULT_VIEW_DIST,Polygon_models[mn].rad,BASE_MODEL_SIZE);
	else
		temp_pos.z = DEFAULT_VIEW_DIST;

	vm_angles_2_matrix(&temp_orient, orient_angles);

	//save_light = Lighting_on;
	//Lighting_on = 0;
	draw_polygon_model(&temp_pos,&temp_orient,NULL,mn,0,f1_0,NULL,NULL);
	//Lighting_on = save_light;

	gr_set_current_canvas(save_canv);

	gr_bitmap(0,0,&temp_canv->cv_bitmap);

	gr_free_canvas(temp_canv);
}
