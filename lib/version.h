#ifndef __version__
	#if defined(DEBUG)
		#define __version__ "3.1.4-debug"
	#elif defined(NDEBUG)
		#define __version__ "3.1.4-release"
	#else
		#define __version__ "3.1.4"
	#endif
#endif
