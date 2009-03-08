/* Handle clipboard text and data in arbitrary formats */

/* Miscellaneous defines */
#define T(A, B, C, D)	(int)((A<<24)|(B<<16)|(C<<8)|(D<<0))

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern int init_scrap(void);
extern int lost_scrap(void);
extern void put_scrap(int type, int srclen, const char *src);
extern void get_scrap(int type, int *dstlen, char **dst);
extern int clipboard_filter(const SDL_Event *event);

#ifdef __cplusplus
}
#endif /* __cplusplus */
