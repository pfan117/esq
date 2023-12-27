#ifndef __ESQ_INTERNAL_LOG_PRINT_H_INCLUDED__
#define __ESQ_INTERNAL_LOG_PRINT_H_INCLUDED__

extern int esq_log_print_enabled;

#if defined PRINT_DBG_LOG
	#define PRINTF(...)	esq_log_print_enabled ? printf(__VA_ARGS__) : 0
#else
	#define PRINTF(...)
#endif

#if defined PRINT_DBG_LOG
#define SHOW_EVENT_BITS(__EVT__)	do	{ \
		if (esq_log_print_enabled)	{ \
			char __event_bits[64]; \
			PRINTF("DBG: %s() %d: event bits are 0x%x <%s>\n" \
					, __func__, __LINE__ \
					, __EVT__ \
					, esq_get_event_bits_string(__EVT__, __event_bits, sizeof(__event_bits)) \
					); \
			} \
		} while(0);
#else
#define SHOW_EVENT_BITS(__EVT__)
#endif

#endif
