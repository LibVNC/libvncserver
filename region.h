/*#define KeySym ___KeySym
#define Bool ___Bool
#define _Box ___Box
#define BoxRec ___BoxRec
#define _RegData ___RegData
#define RegDataRec ___RegDataRec
#define RegDataPtr ___RegDataPtr
#define _Region ___Region
#define RegionRec ___RegionRec
#define RegionPtr ___RegionPtr
#define CARD8 ___CARD8
#define CARD16 ___CARD16
#define CARD32 ___CARD32
#include "X11/Xalloca.h"
#include "Xserver/regionstr.h"
#undef KeySym
#undef Bool
#undef _Box
#undef BoxRec
#undef _RegData
#undef RegDataRec
#undef RegDataPtr
#undef _Region
#undef RegionRec
#undef RegionPtr
#undef CARD8
#undef CARD16
#undef CARD32

#undef REGION_INTERSECT
#undef REGION_UNION
#undef REGION_SUBTRACT
#undef REGION_TRANSLATE
#undef REGION_INIT
#undef REGION_UNINIT
#undef REGION_EMPTY
#undef REGION_NOTEMPTY
#undef FALSE
#undef TRUE
*/

extern void miRegionInit(RegionPtr,BoxPtr,int);
extern void miRegionUninit(RegionPtr);
extern Bool miRegionEmpty(RegionPtr);
extern Bool miRegionNotEmpty(RegionPtr);
extern Bool miIntersect(RegionPtr,RegionPtr,RegionPtr);
extern Bool miSubtract(RegionPtr,RegionPtr,RegionPtr);
extern Bool miUnion(RegionPtr,RegionPtr,RegionPtr);
extern void miTranslateRegion(RegionPtr,int,int);
