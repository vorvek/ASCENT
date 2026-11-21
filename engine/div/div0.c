/*
 * ASCENT: C replacement for div0.asm (divide exception handler).
 *
 * div0.asm installed a DPMI handler for the processor divide exception
 * (divide by zero and quotient overflow). The handler looked up the address
 * of the divide instruction in two lists: a list of callbacks and a list of
 * instructions whose result to saturate. For an address in no list it used
 * the default mode: DM_SATURATE (saturate the result) or DM_ERROR (remove
 * the handler, so that the exception ends the program).
 *
 * The port has no divide exception to handle. The fixed point divides
 * (fixdiv and fixmuldiv in fix.h) test the divisor and the quotient and
 * saturate the result. These functions keep the interface of div0.h and
 * report success, but they install nothing:
 *  - the mode has no effect. The game calls div0_init(DM_ERROR), and a
 *    divide overflow stopped the program. Here it saturates.
 *  - the addresses for div0_set_handler() and div0_set_saturate() are not
 *    stored and a callback is never called.
 *  - the three counters stay at zero.
 * div0.asm stays in the tree as the reference. It is not built.
 */

#include "div0.h"

int div0_num_handled_by_cblist = 0;
int div0_num_handled_by_satlist = 0;
int div0_num_saturated = 0;

static int div0_mode = DM_ERROR;

//Returns 1=Installed OK, 0=Failed.
int div0_init(int mode)
{
	div0_mode = mode;

	return 1;
}

void div0_close()
{
}

void div0_set_mode(int mode)
{
	div0_mode = mode;
}

//Returns 1 if ok, and 0 if there isn't any more "slots" available.
int div0_set_handler(void *div_addr,void *handler_addr)
{
	(void) div_addr;
	(void) handler_addr;

	return 1;
}

//Returns 1 if ok, and 0 if there isn't any more "slots" available.
int div0_set_saturate(void *div_addr)
{
	(void) div_addr;

	return 1;
}
