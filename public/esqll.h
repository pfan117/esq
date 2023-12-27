#ifndef __ESQ_LINK_LIST_MACRO_HEADER_INCLUDED__
#define __ESQ_LINK_LIST_MACRO_HEADER_INCLUDED__

/* dbg_print("debug: %s, %d: %s\n", __func__, __LINE__, ""); */
#ifdef DEBUG
#define dbg_print(...)	printf(__VA_ARGS__)
#else
#define dbg_print(...)
#endif

#define DOUBLE_LL_JOIN_HEAD(__head__,__tail__,__prev__,__next__,__node__)	do	{	\
	(__node__)-> __prev__ = NULL;	\
	if (__head__)	{	\
		(__node__)-> __next__ = (__head__);	\
		(__head__)-> __prev__ = (__node__);	\
		(__head__) = __node__;	\
	}	\
	else	{	\
		(__node__)-> __next__ = NULL;	\
		(__head__) = (__node__);	\
		(__tail__) = (__node__);	\
	}	\
}	\
while(0);

#define DOUBLE_LL_JOIN_TAIL(__head__,__tail__,__prev__,__next__,__node__)	do	{	\
	(__node__)-> __next__ = NULL;	\
	if (__tail__)	{	\
		(__node__)-> __prev__ = (__tail__);	\
		(__tail__)-> __next__ = (__node__);	\
		(__tail__) = (__node__);	\
	}	\
	else	{	\
		(__node__)-> __prev__ = NULL;	\
		(__head__) = (__node__);	\
		(__tail__) = (__node__);	\
	}	\
}	\
while(0);

#define DOUBLE_LL_LEFT(__head__,__tail__,__prev__,__next__,__node__)	do	{	\
	if ((__node__)-> __prev__)	{	\
		if ((__node__)-> __next__)	{	\
			(__node__)-> __prev__ -> __next__ = (__node__)-> __next__;	\
			(__node__)-> __next__ -> __prev__ = (__node__)-> __prev__;	\
		}	\
		else	{	\
			(__node__)-> __prev__-> __next__ = NULL;	\
			(__tail__) = __node__-> __prev__;	\
		}	\
	}	\
	else	{	\
		if ((__node__)-> __next__)	{	\
			(__node__)-> __next__ -> __prev__ = NULL;	\
			(__head__) = __node__ -> __next__;	\
		}	\
		else	{	\
			(__head__) = NULL;	\
			(__tail__) = NULL;	\
		}	\
	}	\
}	\
while(0);

#define SINGLE_LL_JOIN_HEAD(__head__,__tail__,__prev__,__next__,__node__)	do	{	\
	if ((__head__))	{	\
		(__node__)-> __next__ = (__head__);	\
		(__head__) = (__node__);	\
	}	\
	else	{	\
		(__head__) = (__node__);	\
		(__tail__) = (__node__);	\
		(__node__)-> __next__ = NULL;	\
	}	\
}	\
while(0);

#define SINGLE_LL_JOIN_TAIL(__head__,__tail__,__prev__,__next__,__node__)	do	{	\
	if ((__tail__))	{	\
		(__tail__)-> __next__ = (__node__);	\
		(__tail__) = (__node__);	\
		(__node__)-> __next__ = NULL;	\
	}	\
	else	{	\
		(__head__) = (__node__);	\
		(__tail__) = (__node__);	\
		(__node__)-> __next__ = NULL;	\
	}	\
}	\
while(0);

#endif /* __ESQ_INCLUDED_MISC__ */
