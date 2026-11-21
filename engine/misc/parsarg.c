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
 * $Source: f:/miner/source/misc/rcs/parsarg.c $
 * $Revision: 1.1 $
 * $Author: matt $
 * $Date: 1993/09/09 17:32:03 $
 *
 * Source for parse_args() function.  See header file (parsarg.h) for info
 *
 * $Log: parsarg.c $
 * Revision 1.1  1993/09/09  17:32:03  matt
 * Initial revision
 * 
 *
 */

// ASCENT: the Watcom "#pragma off (unreferenced)" lines are removed.
static char rcsid[] = "$Id: parsarg.c 1.1 1993/09/09 17:32:03 matt Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

// ASCENT: dos.h (_dos_findfirst, _dos_findnext, _splitpath) is replaced with
// dirent.h. A compiler without dirent.h (MSVC) does not expand wildcards.
#ifndef _MSC_VER
#include <dirent.h>
#include <sys/stat.h>
#define PA_HAVE_DIRENT
#endif

#include "parsarg.h"

#define ARGBUF_SIZE 500
#define MAX_ARGS 100
#define PA_MAX_PATH 512

char arg_buf[ARGBUF_SIZE];
char *ab_ptr=arg_buf;

void perror_exit(int n,char *s)
{
	perror(s);
	exit(n);
}

// ASCENT: takes a va_list. The original took the address of the next
// parameter on the stack.
void verror(char *s,va_list args)
{
	fprintf(stderr,"Error: ");
	vfprintf(stderr,s,args);
	fprintf(stderr,"\n");
}

void error_exit(int ret_code,char *s,...)
{
	va_list args;

	va_start(args,s);
	verror(s,args);
	va_end(args);
	exit(ret_code);
}

#ifdef PA_HAVE_DIRENT

// ASCENT: wildcard match with the DOS rules: '*' and '?', and the case of
// the letters is ignored.
static int pa_wild_match(const char *pat,const char *name)
{
	while (*pat) {
		if (*pat=='*') {
			pat++;
			do {
				if (pa_wild_match(pat,name)) return 1;
			} while (*name++);
			return 0;
		}
		if (!*name) return 0;
		if (*pat!='?' && tolower(*pat)!=tolower(*name)) return 0;
		pat++; name++;
	}
	return *name==0;
}

//calls the handler for each file that matches the spec. returns the number of matches
static int pa_expand(char *spec,void (*handler_func)(char *arg))
{
	char filename[PA_MAX_PATH],pattern[PA_MAX_PATH],*nptr,*p,*sep;
	DIR *dir;
	struct dirent *ent;
	struct stat st;
	int count=0,plen,no_ext;

	if (strlen(spec) >= PA_MAX_PATH) return 0;

	sep = NULL;											//find end of path part
	for (p=spec;*p;p++)
		if (*p=='/' || *p=='\\' || *p==':') sep = p;

	if (sep) {
		memcpy(filename,spec,sep+1-spec);
		filename[sep+1-spec] = 0;
		strcpy(pattern,sep+1);
	}
	else {
		filename[0] = 0;
		strcpy(pattern,spec);
	}
	nptr = filename + strlen(filename);			//point at name part

	//in DOS, "name.*" also matches a name that has no extension
	plen = (int) strlen(pattern);
	no_ext = (plen>=2 && pattern[plen-2]=='.' && pattern[plen-1]=='*');

	if ((dir=opendir(filename[0]?filename:"."))==NULL) return 0;

	while ((ent=readdir(dir))!=NULL) {
		int match;

		if (!strcmp(ent->d_name,".") || !strcmp(ent->d_name,"..")) continue;
		if (strlen(filename)+strlen(ent->d_name) >= PA_MAX_PATH) continue;

		match = pa_wild_match(pattern,ent->d_name);
		if (!match && no_ext && !strchr(ent->d_name,'.')) {
			pattern[plen-2] = 0;
			match = pa_wild_match(pattern,ent->d_name);
			pattern[plen-2] = '.';
		}
		if (!match) continue;

		strcpy(nptr,ent->d_name);					//copy name after path

		if (stat(filename,&st)==0 && S_ISDIR(st.st_mode)) continue;	//files only

		handler_func(filename);
		count++;
	}

	closedir(dir);

	return count;
}

#endif

void parse_args(int argc,char **argv,void (*handler_func)(char *arg),int flags)
{

	for (;argc--;argv++) {
		if (**argv=='@') {			/* read args from file */
			char *arg_ptrs[MAX_ARGS];
			int arg_count;
			FILE *argfile;
			int len;
			char *p=ab_ptr,c;

			if ((argfile=fopen(*argv+1,"rt"))==0) perror_exit(10,*argv+1);
			if ((len=fread(ab_ptr,1,ARGBUF_SIZE-((int) (ab_ptr-arg_buf)),argfile))==ARGBUF_SIZE-(ab_ptr-arg_buf)) error_exit(20,"Argument buffer not big enough\n");
			fclose(argfile);
			ab_ptr[len++]=0;		/* write terminating null */

			/* remove comments */

			while ((p=strchr(ab_ptr,';'))!=NULL) {
				char *p2=strchr(p,'\n');

				if (p2) { 	/* found cr */
					// ASCENT: memmove, because the two strings overlap
					memmove(p,p2,strlen(p2)+1);	/* copy over comment */
					len = strlen(ab_ptr);
				}
				else {		/* no cr, end of string */
					*p=0;
					len = (int) (p-ab_ptr);
				}
			}
			ab_ptr[len]=0;		/* write terminating null */

			// ASCENT: the test of len stops the loop for an empty file
			while (len>0 && !ab_ptr[len-1]) len--;	/* back up over terminating nulls */
			p=ab_ptr;


			for (arg_count=0;p<ab_ptr+len;) {
				// ASCENT: '\r' is also white space (a DOS file that is read on another system)
				while (p<ab_ptr+len && ((c=*p)==' ' || c=='\t' || c=='\n' || c=='\r')) p++;
				if (p<ab_ptr+len) {	/* found parm? */
					arg_ptrs[arg_count++]=p;
					if (arg_count>=MAX_ARGS) error_exit(10,"Too many args");
					while (p<ab_ptr+len && !((c=*p)==' ' || c=='\t' || c=='\n' || c=='\r')) p++;
					*p++=0;
				}
			}
			ab_ptr+=len+1;			// ASCENT: +1 keeps the null of the last arg
			parse_args(arg_count,arg_ptrs,handler_func,flags);
		}
		else
			if (flags&PA_EXPAND && (**argv != '-')) {
				// ASCENT: the directory search uses dirent.h, see pa_expand().
				// A spec without a wildcard goes to the handler as it is.
				int n_found=0;

#ifdef PA_HAVE_DIRENT
				if (strchr(*argv,'*') || strchr(*argv,'?'))
					n_found = pa_expand(*argv,handler_func);
#endif

				if (!n_found) handler_func(*argv);

			}
			else
				handler_func(*argv);

	}

}
