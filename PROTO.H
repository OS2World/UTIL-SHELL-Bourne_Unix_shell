/*
 * Prototype definitions for Standard and Non-standard compilers
 */

#undef _PROTO
#undef _CDECL
#undef _NEAR

#ifdef MSDOS
#  ifndef NO_EXT_KEYS				/* extensions enabled */
#    define _CDECL	cdecl
#    define _NEAR	near
#  else
#    define _CDECL
#    define _NEAR
#  endif
#else
#    define _CDECL
#endif

#ifdef __STDC__
#  define _PROTO(p)	p
#else
#  define _PROTO(p)	()
#  undef  const
#endif
