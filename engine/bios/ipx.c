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
 * $Source: f:/miner/source/bios/rcs/ipx.c $
 * $Revision: 2.10 $
 * $Author: john $
 * $Date: 1995/03/29 17:27:55 $
 * 
 * Routines for IPX communications.
 * 
 * $Log: ipx.c $
 * Revision 2.10  1995/03/29  17:27:55  john
 * Added code to not duplicate broadcasts.
 * 
 * Revision 2.9  1995/03/29  15:32:07  john
 * NEatened.
 * 
 * Revision 2.8  1995/03/29  15:07:31  john
 * Added network support.
 * 
 * Revision 2.7  1995/03/29  11:22:27  john
 * Added broadcasting over a net.
 * 
 * Revision 2.6  1995/03/28  20:05:03  john
 * Took away alternate server stuff.
 * 
 * Revision 2.5  1995/03/23  19:14:29  john
 * Added better MAX_USERS message.
 * 
 * Revision 2.4  1995/03/23  19:00:02  john
 * Added user list capabitly.
 * 
 * Revision 2.3  1995/03/23  12:27:05  john
 * Move IPX into bios lib.
 * 
 * Revision 2.2  1995/03/22  19:08:15  john
 * Added code to fix sending packets over router... now
 * we just need to make broadcasts go over router!!
 * 
 * Revision 2.1  1995/03/21  08:40:16  john
 * Ifdef'd out the NETWORK code.
 * 
 * Revision 2.0  1995/02/27  11:30:45  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.31  1995/02/16  17:35:03  john
 * Added code to allow dynamic socket changing.
 * 
 * Revision 1.30  1995/02/01  21:03:45  john
 * Lintified.
 * 
 * Revision 1.29  1995/01/22  14:31:05  john
 * Added key.h
 * 
 * Revision 1.28  1995/01/22  14:29:26  john
 * Added code to bypass broadcast messages.
 * 
 * Revision 1.27  1995/01/03  13:46:16  john
 * Added code that should make ipx work over different servers,
 * but ifdef'd it out with SHAREWARE in ipx.c.  I haven't tested
 * this, and I hope it doesn't introduce net bugs.
 * 
 * Revision 1.26  1994/12/14  11:54:37  rob
 * Reduce # of ipx packets.
 * 
 * Revision 1.25  1994/12/11  00:43:28  john
 * Locked down DOS ipx buffers... probably not necessary, tho.
 * 
 * Revision 1.24  1994/12/08  15:49:58  rob
 * Bumped up the # of ipx packets.
 * 
 * Revision 1.23  1994/12/04  15:23:54  rob
 * Reduced packet # constants.
 * 
 * Revision 1.22  1994/11/27  23:12:39  matt
 * Made changes for new mprintf calling convention
 * 
 * Revision 1.21  1994/11/19  15:15:19  mike
 * remove unused code and data
 * 
 * Revision 1.20  1994/11/10  11:06:07  rob
 * Raised number of ipx packets to 64 and buffers to 128 (for 8-way games)
 * Removed (commented out) all the spx support stuff.
 * 
 * Revision 1.19  1994/11/07  17:50:12  rob
 * Bumped up max_packets and ipx_num_packets to double their previous
 * values.
 * 
 * Revision 1.18  1994/11/07  15:39:01  rob
 * Removed temporary hacked ipx_send_object function.
 * Replaced with a more general thing.
 * 
 * Revision 1.17  1994/11/05  16:08:40  rob
 * Made a change necessary for object syncing on rejoin.
 * 
 * Revision 1.16  1994/11/01  15:57:12  rob
 * Added support for SPX sockets for critical messages.
 * 
 * Revision 1.15  1994/10/31  19:21:01  rob
 * Added a routine for sending objects via ipx.  Its a hack and shoukld
 * be replaced by something more general time permitting.
 * 
 * Revision 1.14  1994/10/20  14:57:42  john
 * Some test stuff.
 * 
 * Revision 1.13  1994/10/20  11:22:45  john
 * Tweaked the number of waiting packets. Added code to print 
 * total packets missed, and to limit the packet send rate to
 * 10 packets per second unless firing.
 * 
 * Revision 1.12  1994/10/03  20:50:11  john
 * Started adding sound pause functions.
 * 
 * Revision 1.11  1994/09/15  08:54:24  mike
 * comment out a mprintf.
 * 
 * Revision 1.10  1994/09/12  19:38:20  john
 * Made some stuff that prints to the DOS screen go to the
 * mono instead, since it really is debugging info.
 * 
 * Revision 1.9  1994/09/07  13:32:03  john
 * Made ipx_close explicitly close the socket we were using,
 * because it seems that VLM's don't automatically close them
 * like the older NETX stuff did (Going from Novell 3.11 driver
 * to 3.12 anyway).
 * 
 * Revision 1.8  1994/09/06  19:29:20  john
 * Added trial version of rejoin function.
 * 
 * Revision 1.7  1994/08/09  19:31:50  john
 * Networking changes.
 * 
 * Revision 1.6  1994/08/05  16:11:41  john
 * Psuedo working version of networking.
 * 
 * Revision 1.5  1994/08/04  19:17:21  john
 * Inbetween version of network stuff.
 * 
 * Revision 1.4  1994/07/29  16:08:57  john
 * *** empty log message ***
 * 
 * Revision 1.3  1994/07/25  12:33:31  john
 * Network "pinging" in.
 * 
 * Revision 1.2  1994/07/20  15:58:28  john
 * First installment of ipx stuff.
 * 
 * Revision 1.1  1994/07/19  15:42:00  john
 * Initial revision
 * 
 * 
 */


static char rcsid[] = "$Id: ipx.c 2.10 1995/03/29 17:27:55 john Exp $";

// ASCENT: the original called the real mode IPX driver (INT 2Fh, INT 7Ah)
// through DPMI. There is no IPX driver. ipx_init reports "IPX not
// installed" (-3), which the game handles: it turns the network game off.
// The other functions do nothing. The original code is in
// reference/d1src/BIOS/IPX.C.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "ipx.h"

static ubyte ipx_my_node[6];
static ubyte ipx_my_server[4];

ubyte * ipx_get_my_local_address()
{
	return ipx_my_node;
}

ubyte * ipx_get_my_server_address()
{
	return ipx_my_server;
}

void ipx_get_local_target( ubyte * server, ubyte * node, ubyte * local_target )
{
	memset( local_target, 0, 6 );
}

int ipx_get_packet_data( ubyte * data )
{
	return 0;
}

void ipx_send_packet_data( ubyte * data, int datasize, ubyte *network, ubyte *address, ubyte *immediate_address )
{
}

void ipx_send_broadcast_packet_data( ubyte * data, int datasize )
{
}

void ipx_send_internetwork_packet_data( ubyte * data, int datasize, ubyte * server, ubyte *address )
{
}

int ipx_change_default_socket( ushort socket_number )
{
	return -3;
}

//---------------------------------------------------------------
// Initializes all IPX internals.
// If socket_number==0, then opens next available socket.
// Returns:	0  if successful.
//				-1 if socket already open.
//				-2	if socket table full.
//				-3 if IPX not installed.
//				-4 if couldn't allocate low dos memory
//				-5 if error with getting internetwork address
int ipx_init( int socket_number, int show_address )
{
	return -3;
}

void ipx_read_user_file(char * filename)
{
}

void ipx_read_network_file(char * filename)
{
}
