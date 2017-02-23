/*
 *  File:
 *    stkwalk.h
 *
 *  Original author:
 *    Jochen Kalmbach
 *
 *  Heavily modified by:
 *    Gisle Vanem
 */

#ifndef __STACKWALKER_H__
#define __STACKWALKER_H__

extern BOOL  StackWalkInit (void);
extern BOOL  StackWalkExit (void);
extern char *StackWalkShow (HANDLE thread, CONTEXT *ctx);

#endif  /* __STACKWALKER_H__ */
