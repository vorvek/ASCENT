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
 * $Source: f:/miner/source/misc/rcs/error.h $
 * $Revision: 1.12 $
 * $Author: matt $
 * $Date: 1994/06/17 15:22:46 $
 *
 * Header for error handling/printing/exiting code
 *
 * $Log: error.h $
 * Revision 1.12  1994/06/17  15:22:46  matt
 * Added pragma for Error() for when NDEBUG
 * 
 * Revision 1.11  1994/03/07  13:22:14  matt
 * Since the Error() function has 'aborts' set in pragma, we do a jmp
 * to the function rather than call.
 * 
 * Revision 1.10  1994/02/17  12:37:15  matt
 * Combined two pragma's for Error(), since second superseded the first
 * 
 * Revision 1.9  1994/02/10  18:02:53  matt
 * Changed 'if DEBUG_ON' to 'ifndef NDEBUG'
 * 
 * Revision 1.8  1994/02/09  15:18:29  matt
 * Added pragma saying that Error() never returns
 * 
 * Revision 1.7  1993/10/19  12:57:53  matt
 * If DEBUG_ON not defined, define it to be 1
 * 
 * Revision 1.6  1993/10/15  21:40:39  matt
 * Made error functions generate int3's if debugging on
 * 
 * Revision 1.5  1993/10/14  15:29:22  matt
 * Added new function clear_warn_func()
 * 
 * Revision 1.4  1993/10/08  16:16:47  matt
 * Made Assert() call function _Assert(), rather to do 'if...' inline.
 * 
 * Revision 1.3  1993/09/29  11:39:07  matt
 * Added Assert() macro, like the system one, but calls Error()
 * 
 * Revision 1.2  1993/09/27  11:47:03  matt
 * Added function set_warn_func()
 * 
 * Revision 1.1  1993/09/23  20:17:46  matt
 * Initial revision
 * 
 *
 */

#ifndef _ERROR_H
#define _ERROR_H

// ASCENT: Error() never returns. The Watcom form was "#pragma aux Error aborts".
#if defined(__GNUC__) || defined(__clang__)
#define ERROR_NORETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
#define ERROR_NORETURN __declspec(noreturn)
#else
#define ERROR_NORETURN
#endif

int error_init(char *fmt,...);			//init error system, set default message, returns 0=ok
void set_exit_message(char *fmt,...);	//specify message to print at exit
void Warning(char *fmt,...);				//print out warning message to user
void set_warn_func(void (*f)(char *s));//specifies the function to call with warning messages
void clear_warn_func(void (*f)(char *s));//say this function no longer valid
void _Assert(int expr,char *expr_text,char *filename,int linenum);	//assert func
ERROR_NORETURN void Error(char *fmt,...);					//exit with error code=1, print message

void Assert(int expr);
void Int3();

// ASCENT: hooks for the platform layer and for a debugger.

//Error() calls this function with the message ("Error: ...") before it
//prints the message and exits. The platform layer can show a message box
//in it. Pass NULL to remove the function.
void set_error_func(void (*f)(char *msg));

//Stops in the debugger. Without a debugger the system ends the program.
void debug_break(void);

//When this is 0 (the default), Int3() prints the file and the line and
//the program continues. When it is not 0, Int3() and Error() call
//debug_break(). Set it when a debugger is attached.
extern int Int3_break_enabled;

//the function behind the Int3() macro
void _Int3(char *filename,int linenum);

// ASCENT: the "#pragma aux" lines made Int3 an inline "int 3h" and put an
// "int 3" before the calls of Error() and of _Assert(). Int3() and Assert()
// are now macros. Assert() calls _Assert(), and _Assert() calls Error() when
// the expression is false. With NDEBUG, Assert() still evaluates the
// expression, as the function Assert() of the source release did, because
// some callers depend on it, for example Assert(fseek(...)==0) in iff.c.

#ifndef NDEBUG		//macros for debugging

#define Assert(expr) _Assert((expr)!=0,#expr,__FILE__,__LINE__)
#define Int3() _Int3(__FILE__,__LINE__)

#else					//macros for real game

#define Assert(expr) ((void)(expr))
#define Int3() ((void)0)

#endif

#endif
