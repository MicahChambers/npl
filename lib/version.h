#ifndef __version__
	#if defined(DEBUG)
		#define __version__ "3.0.5-debug"
	#elif defined(NDEBUG)
		#define __version__ "3.0.5-release"
	#else
		#define __version__ "3.0.5"
	#endif
#endif
