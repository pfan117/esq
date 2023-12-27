#include <stdio.h>
#ifdef __linux__
	#define __USE_GNU
#endif
#include <dlfcn.h>
#include <monkeycall.h>
#include <string.h>
#include <strings.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "public/esqll.h"
#include "internal/monkeycall.h"
#include "internal/modules.h"

#define ESQ_MODULE_MAX	16

static void * esq_module_handle_list[ESQ_MODULE_MAX];
static esq_module_detach_cb_t esq_module_detach_list[ESQ_MODULE_MAX];

void
esq_trigger_mt(void)	{
	esq_mt_start_t test_cb;

	test_cb = (esq_mt_start_t)dlsym(RTLD_DEFAULT, "esq_mt_start");
	if (test_cb)	{
		test_cb();
	}

	return;
}

int
esq_modules_load(const char * filename)	{
	esq_module_init_cb_t init;
	esq_module_detach_cb_t detach;
	void * handle;
	int r;
	int i;

	for (i = 0; i < ESQ_ARRAY_SIZE(esq_module_handle_list); i ++)	{
		if (!esq_module_handle_list[i])	{
			break;
		}
	}

	if (i >= ESQ_ARRAY_SIZE(esq_module_handle_list))	{
		fprintf(stderr, "error: %s, %d: can't load more than %d modules\n", __func__, __LINE__, ESQ_MODULE_MAX);
		return ESQ_ERROR_SIZE;
	}

	handle = dlopen(filename, RTLD_NOW | RTLD_GLOBAL);
	if (!handle)	{
		fprintf(stderr, "error: %s, %d: dlopen() returned error (%s)\n"
				, __func__, __LINE__
				, dlerror()
				);
		return ESQ_ERROR_LIBCALL;
	}

	init = (esq_module_init_cb_t)dlsym(handle, "__esq_mod_init");
	if (init)	{
		r = init();
		if (r)	{
			fprintf(stderr, "error: %s, %d: __esq_mod_init() return failure\n", __func__, __LINE__);
			dlclose(handle);
			return ESQ_ERROR_LIBCALL;
		}
	}

	detach = (esq_module_detach_cb_t)dlsym(handle, "__esq_mod_detach");

	esq_module_handle_list[i] = handle;
	esq_module_detach_list[i] = detach;

	return ESQ_OK;
}

int
esq_modules_init(void)	{
	bzero(esq_module_handle_list, sizeof(esq_module_handle_list));
	bzero(esq_module_detach_list, sizeof(esq_module_detach_list));
	return ESQ_OK;
}

void
esq_modules_detach(void)	{
	esq_module_detach_cb_t detach;
	void * p;
	int i;

	for (i = (ESQ_ARRAY_SIZE(esq_module_detach_list) - 1); i >= 0; i --)	{
		detach = esq_module_detach_list[i];
		if (detach)	{
			detach();
		}
	}

	for (i = 0; i < ESQ_ARRAY_SIZE(esq_module_handle_list); i ++)	{
		p = esq_module_handle_list[i];
		if (p)	{
			dlclose(p);
		}
	}

	return;
}

/* eof */
