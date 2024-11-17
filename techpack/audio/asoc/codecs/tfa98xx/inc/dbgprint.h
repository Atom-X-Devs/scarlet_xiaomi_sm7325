/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2021 GOODIX
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _DBGPRINT_H
#define _DBGPRINT_H

/* Debugging macro's. */
#ifndef DEBUG
#define DEBUG
#endif

//TODO wwwim
#ifndef _ASSERT
#define _ASSERT(e)
#endif

#ifndef PREFIX
#define PREFIX "tfa98xx: "
#define DRIVER_NAME "tfa98xx"
#endif

#ifdef DEBUG
#define _DEBUG(level, fmt, va...) do {    \
if (unlikely(debug >= (level)))		\
	printk(KERN_INFO PREFIX "%s: %d: "fmt, __func__, __LINE__, ##va); \
} while (0)
#else
#define _DEBUG(level, fmt, va...) do {} while(0)
#endif

#define MSG(fmt, va...) printk(KERN_INFO PREFIX "%s: %d: "fmt, __func__, __LINE__, ##va)
#define _ERRORMSG(fmt, va...) printk(KERN_ERR PREFIX "ERROR %s: %d: "fmt, __func__, __LINE__, ##va)

#define DEBUG0(x...) MSG(x)
#define DEBUG1(x...) _DEBUG(1,x)
#define DEBUG2(x...) _DEBUG(2,x)
#define DEBUG3(x...) _DEBUG(3,x)
#define ERRORMSG(x...) _ERRORMSG(x)

#define PRINT(x...)	printk(x)
#define PRINT_ERROR(x...) printk(KERN_INFO PREFIX "**ERROR**" x)
#define PRINT_ASSERT(e)if ((e)) printk(KERN_ERR "PrintAssert: %s (%s: %d) error code: %d\n", __FUNCTION__, __FILE__, __LINE__, e)

#define PRINT_ENTRY DEBUG2("+[%s]\n", __func__)
#define PRINT_EXIT  DEBUG2("-[%s]\n", __func__)

#ifdef ASSERT
#define assert(cond,action) do {	\
if (unlikely(!(cond)))			\
	DEBUG0("Assert: %s\n",#cond); action; \
} while(0)
#else
#define assert(cond,action) do { } while (0)
#endif

#endif /* _DBGPRINT_H */
