/* create grk_config_private.h for CMake */
#define GROK_HAVE_INTTYPES_H 	@GROK_HAVE_INTTYPES_H@

#define GRK_PACKAGE_VERSION "@PACKAGE_VERSION@"

#define _LARGEFILE_SOURCE
#define _LARGE_FILES
#define _FILE_OFFSET_BITS @_FILE_OFFSET_BITS@
#define GROK_HAVE_FSEEKO @GROK_HAVE_FSEEKO@

/* find whether or not have <malloc.h> */
#define GROK_HAVE_MALLOC_H
/* check if function `aligned_alloc` exists */
#define GROK_HAVE_ALIGNED_ALLOC
/* check if function `_aligned_malloc` exists */
#define GROK_HAVE__ALIGNED_MALLOC
/* check if function `memalign` exists */
#define GROK_HAVE_MEMALIGN
/* check if function `posix_memalign` exists */
#define GROK_HAVE_POSIX_MEMALIGN

#if !defined(_POSIX_C_SOURCE)
#if defined(GROK_HAVE_FSEEKO) || defined(GROK_HAVE_POSIX_MEMALIGN)
/* Get declarations of fseeko, ftello, posix_memalign. */
#define _POSIX_C_SOURCE 200112L
#endif
#endif

/* Byte order.  */
/* All compilers that support Mac OS X define either __BIG_ENDIAN__ or
__LITTLE_ENDIAN__ to match the endianness of the architecture being
compiled for. This is not necessarily the same as the architecture of the
machine doing the building. In order to support Universal Binaries on
Mac OS X, we prefer those defines to decide the endianness.
On other platforms we use the result of the TRY_RUN. */
#if !defined(__APPLE__)
#define GROK_BIG_ENDIAN
#elif defined(__BIG_ENDIAN__)
# define GROK_BIG_ENDIAN
#endif
