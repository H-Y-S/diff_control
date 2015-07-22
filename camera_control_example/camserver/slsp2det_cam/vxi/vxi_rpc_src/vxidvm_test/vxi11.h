/******************************************************************************
 *
 * $RCSfile: vxi11.h,v $ (This file is best viewed with a tabwidth of 4)
 *
 ******************************************************************************
 *
 * Author: Benjamin Franksen
 *
 *		Constants from VXI-11 specification
 *
 * $Revision: 1.1.1.1 $
 *
 * $Date: 1997/04/25 13:36:06 $
 *
 * $Author: franksen $
 *
 * $Log: vxi11.h,v $
 * Revision 1.1.1.1  1997/04/25 13:36:06  franksen
 * Imported using tkCVS
 *
 *
 * Copyright (c) 1996  Berliner Elektronenspeicherring-Gesellschaft
 *                           fuer Synchrotronstrahlung m.b.H.,
 *                                   Berlin, Germany
 *
 ******************************************************************************
 *
 * Notes:
 *
 *	This is not complete yet.
 *
 */
#ifndef VXI11_H
#define VXI11_H

/* VXI-11 error codes */

#define VXI_OK           0   /* no error */
#define VXI_SYNERR       1   /* syntax error */
#define VXI_NOACCESS     3   /* device not accessible */
#define VXI_INVLINK      4   /* invalid link identifier */
#define VXI_PARAMERR     5   /* parameter error */
#define VXI_NOCHAN       6   /* channel not established */
#define VXI_NOTSUPP      8   /* operation not supported */
#define VXI_NORES        9   /* out of resources */
#define VXI_DEVLOCK      11  /* device locked by another link */
#define VXI_NOLOCK       12  /* no lock held by this link */
#define VXI_IOTIMEOUT    15  /* I/O timeout */
#define VXI_IOERR        17  /* I/O error */
#define VXI_INVADDR      21  /* invalid address */
#define VXI_ABORT        23  /* abort */
#define VXI_CHANEXIST    29  /* channel already established */

/* VXI-11 flags  */

#define VXI_WAITLOCK     1   /* block the operation on a locked device */
#define VXI_ENDW         8   /* device_write: mark last char with END indicator */
#define VXI_TERMCHRSET   128 /* device_read: stop on termination character */

/* VXI-11 read termination reasons */

#define VXI_REQCNT       1   /* requested # of bytes have been transferred */
#define VXI_CHR          2   /* termination character matched */
#define VXI_ENDR         4   /* END indicator read */

#endif
