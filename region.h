#ifndef REGION_H
#define REGION_H

#include "sraRegion.h"

#if 0
#define NullRegion ((RegionPtr)0)
#define NullBox ((BoxPtr)0)

typedef struct RegDataRec {
    long	size;
    long 	numRects;
/*  BoxRec	rects[size];   in memory but not explicitly declared */
} RegDataRec;

extern void miRegionInit(RegionPtr,BoxPtr,int);
extern void miRegionUninit(RegionPtr);
extern Bool miRegionEmpty(RegionPtr);
extern Bool miRegionNotEmpty(RegionPtr);
extern Bool miIntersect(RegionPtr,RegionPtr,RegionPtr);
extern Bool miSubtract(RegionPtr,RegionPtr,RegionPtr);
extern Bool miUnion(RegionPtr,RegionPtr,RegionPtr);
extern void miTranslateRegion(RegionPtr,int,int);

#define REGION_NIL(reg) ((reg)->data && !(reg)->data->numRects)
#define REGION_NUM_RECTS(reg) ((reg)->data ? (reg)->data->numRects : 1)
#define REGION_SIZE(reg) ((reg)->data ? (reg)->data->size : 0)
#define REGION_RECTS(reg) ((reg)->data ? (BoxPtr)((reg)->data + 1) \
			               : &(reg)->extents)
#define REGION_BOXPTR(reg) ((BoxPtr)((reg)->data + 1))
#define REGION_BOX(reg,i) (&REGION_BOXPTR(reg)[i])
#define REGION_TOP(reg) REGION_BOX(reg, (reg)->data->numRects)
#define REGION_END(reg) REGION_BOX(reg, (reg)->data->numRects - 1)
#define REGION_SZOF(n) (sizeof(RegDataRec) + ((n) * sizeof(BoxRec)))

#define REGION_INIT(s,pReg,rect,size) miRegionInit(pReg,rect,size)
#define REGION_EMPTY(s,pReg) miRegionEmpty(pReg)
#define REGION_UNINIT(s,pReg) miRegionUninit(pReg)
#define REGION_NOTEMPTY(s,pReg) miRegionNotEmpty(pReg)
#define REGION_INTERSECT(s,newReg,reg1,reg2) miIntersect(newReg,reg1,reg2)
#define REGION_SUBTRACT(s,newReg,reg1,reg2) miSubtract(newReg,reg1,reg2)
#define REGION_UNION(s,newReg,reg1,reg2) miUnion(newReg,reg1,reg2)
#define REGION_TRANSLATE(s,pReg,x,y) miTranslateRegion(pReg,x,y)
#endif

#endif
