#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <bfd.h>
#include <demangle.h>
#include <errno.h>
#include <link.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <execinfo.h>

#include "internal/ebacktrace.h"

#define PRINT_LOG 1
#define MAX_CALLTRACE_LEVEL 64

#if defined PRINT_LOG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

typedef struct {
	const asymbol **symbols;
	unsigned long symbol_count;
	bfd_vma address;
	char section_found;
	char symbol_found;
	const char *file_name;
	const char *symbol_name;
	uint line_num;
} address_info_t;

typedef struct {
	char found;
	const void *address;
	const char *file_name;
	void *base;
	void *hdr;
} file_match_info_t;

static const char *exe_filename = NULL;

void
print_symbol_line(void *address, const char *file_name, const char *symbol_name, int line_num) {
	if (file_name == NULL) {
		file_name = "<unknown file>";
	}

	if (symbol_name == NULL) {
		symbol_name = "<unknown symbol>";
	}
	else if (!strcmp("ebacktrace", symbol_name))	{
		return;
	}
	else if (!strcmp("esq_backtrace", symbol_name))	{
		return;
	}

	if (address == ebacktrace)	{
		return;
	}

	printf("[\033[40;34m%p\033[0m] %s \033[40;33m%s\033[0m +%u\n", address, symbol_name, file_name, line_num);
}

bool
read_symbol_table(bfd *abfd, const asymbol **symbols[], unsigned long *symbol_count) {
	uint size;
	long n;

	if ((bfd_get_file_flags(abfd) & HAS_SYMS) == 0) {
		PRINTF("ERROR: no symbol table\n");
		return -1;
	}

	(*symbols) = NULL;
	/* not dynamic */
	n = bfd_read_minisymbols(abfd, FALSE, (void **)symbols, &size);
	if (n == 0) {
		if ((*symbols) != NULL)
			free(*symbols);

		(*symbols) = NULL;
		/* dynamic */
		n = bfd_read_minisymbols(abfd, TRUE, (void **)symbols, &size);
	}
	if (n < 0) {
		PRINTF("ERROR: error reading symbols\n");
		return -1;
	}
	else if (n == 0) {
		PRINTF("ERROR: no symbols found\n");
		return -1;
	}
	(void)size;
	(*symbol_count) = (unsigned long)n;

	return 0;
}

void
free_symbol_table(const asymbol *symbols[], unsigned long symbol_count) {
	free(symbols);
}

bool
demanle_symbol_name(const char *symbol_name, char *demangled_symbol_name, uint demangled_symbol_name_size) {
	char *s;

	s = bfd_demangle(NULL, symbol_name, DMGL_ANSI | DMGL_PARAMS);
	if (s != NULL) {
		strncpy(demangled_symbol_name, s, demangled_symbol_name_size);
		free(s);

		return TRUE;
	}
	else {
		strncpy(demangled_symbol_name, symbol_name, demangled_symbol_name_size);

		return FALSE;
	}
}

static void
find_address_in_section(bfd *abfd, asection *section, void *data) {
	address_info_t *address_info = (address_info_t *)data;
	bfd_vma vma;
	bfd_size_type size;

	if (address_info->symbol_found) {
		return;
	}

	if ((bfd_get_section_flags(abfd, section) & SEC_ALLOC) == 0) {
		return;
	}
	vma = bfd_get_section_vma(abfd, section);
	size = bfd_section_size(abfd, section);
	if ((address_info->address < vma) || (address_info->address >= vma + size)) {
		return;
	}
	address_info->section_found = 1;

	address_info->symbol_found =
		bfd_find_nearest_line(abfd, section, (asymbol **)address_info->symbols, address_info->address - vma,
							  &address_info->file_name, &address_info->symbol_name, &address_info->line_num);
}

int
address_to_symbol_info(bfd *abfd, const asymbol *symbols[], unsigned long symbol_count, bfd_vma address) {
	address_info_t address_info;

	address_info.symbols = symbols;
	address_info.symbol_count = symbol_count;
	address_info.address = address;
	address_info.symbol_found = 0;
	bfd_map_over_sections(abfd, find_address_in_section, (PTR)&address_info);
	if (!address_info.section_found) {
		return 0;
	}
	if (!address_info.symbol_found) {
		return 0;
	}

	while (address_info.symbol_found) {
		char buffer[256];
		const char *symbol_name;
		const char *file_name;

		if ((address_info.symbol_name != NULL) && ((*address_info.symbol_name) != '\0')) {
			if (demanle_symbol_name(address_info.symbol_name, buffer, sizeof(buffer))) {
				symbol_name = buffer;
			}
			else {
				symbol_name = address_info.symbol_name;
			}
		}
		else {
			symbol_name = NULL;
		}

		if (address_info.file_name != NULL) {
			file_name = address_info.file_name;
		}
		else {
			file_name = NULL;
		}

		print_symbol_line((void *)address, file_name, symbol_name, address_info.line_num);

		address_info.symbol_found =
			bfd_find_inliner_info(abfd, &address_info.file_name, &address_info.symbol_name, &address_info.line_num);
	}

	return 1;
}

bfd *
open_bfd(const char *file_name, const asymbol **symbols[], unsigned long *symbol_count) {
	bfd *abfd;
	char **matching;

	abfd = bfd_openr(file_name, NULL);
	if (abfd == NULL) {
		PRINTF("ERROR: can not open file '%s' (error: %s)\n", file_name, strerror(errno));
		return NULL;
	}

	if (bfd_check_format(abfd, bfd_archive)) {
		PRINTF("ERROR: invalid format\n");
		bfd_close(abfd);
		return NULL;
	}

	if (!bfd_check_format_matches(abfd, bfd_object, &matching)) {
		if (bfd_get_error() == bfd_error_file_ambiguously_recognized) {
			free(matching);
		}
		PRINTF("ERROR: format does not match\n");
		bfd_close(abfd);
		return NULL;
	}

	if (read_symbol_table(abfd, symbols, symbol_count)) {
		bfd_close(abfd);
		return NULL;
	}

	return abfd;
}

void
close_bfd(bfd *abfd, const asymbol *symbols[], unsigned long symbol_count) {
	if (!abfd) {
		return;
	}

	free_symbol_table(symbols, symbol_count);
	bfd_close(abfd);
}

bool
get_symbol_info_from_file(const char *file_name, bfd_vma address) {
	bfd *abfd;
	const asymbol **symbols;
	unsigned long symbol_count;
	int result;

	abfd = open_bfd(exe_filename, &symbols, &symbol_count);
	if (!abfd) {
		return 0;
	}

	result = address_to_symbol_info(abfd, symbols, symbol_count, address);

	close_bfd(abfd, symbols, symbol_count);

	return result;
}

int
find_matching_file(struct dl_phdr_info *info, size_t info_size, void *user_data) {
	file_match_info_t *file_match_info = (file_match_info_t *)user_data;
	unsigned long vaddr;
	int i;

	(void)info_size;

	for (i = 0; i < info->dlpi_phnum; i++) {
		if (info->dlpi_phdr[i].p_type == PT_LOAD) {
			vaddr = info->dlpi_addr + info->dlpi_phdr[i].p_vaddr;
			if (((uintptr_t)file_match_info->address >= vaddr) &&
				((uintptr_t)file_match_info->address < vaddr + info->dlpi_phdr[i].p_memsz) &&
				(info->dlpi_name != NULL) && (info->dlpi_name[0] != '\0')) {
				file_match_info->found = 1;
				file_match_info->file_name = info->dlpi_name;
				file_match_info->base = (void *)(uintptr_t)info->dlpi_addr;
			}
		}
	}

	return 0;
}

void
process_backtrace_address(void *addresses) {
	file_match_info_t file_match_info;
	int r;

	file_match_info.found = 0;
	file_match_info.address = addresses;
	dl_iterate_phdr(find_matching_file, &file_match_info);
	if (file_match_info.found) {
		r = get_symbol_info_from_file(file_match_info.file_name,
									  (bfd_vma)((uintptr_t)addresses - (uintptr_t)file_match_info.base));
	}
	else {
		r = get_symbol_info_from_file(exe_filename, (bfd_vma)addresses);
	}

	if (!r) {
		Dl_info info;
		char buffer[256];
		const char *symbol_name;
		const char *file_name;

		if (dladdr(addresses, &info)) {
			if ((info.dli_sname != NULL) && ((*info.dli_sname) != '\0')) {
				if (!demanle_symbol_name(info.dli_sname, buffer, sizeof(buffer))) {
					symbol_name = buffer;
				}
				else {
					symbol_name = info.dli_sname;
				}
			}
			else {
				symbol_name = NULL;
			}
			file_name = info.dli_fname;
		}
		else {
			symbol_name = NULL;
			file_name = NULL;
		}

		print_symbol_line(addresses, file_name, symbol_name, 0);
	}
}

void
ebacktrace_init(const char *filename) {
	exe_filename = filename;
}

int
ebacktrace(void) {
	void *stack_trace[MAX_CALLTRACE_LEVEL];
	int c;
	int i;

	if (!exe_filename) {
		PRINTF("please call ebacktrace_init() first\n");
		return -1;
	}

	c = backtrace(stack_trace, MAX_CALLTRACE_LEVEL);

	if (c <= 0) {
		PRINTF("failed to get backtrace\n");
		return -1;
	}

	for (i = 0; i < c; i++) {
		process_backtrace_address(stack_trace[i]);
	}

	return 0;
}

#if defined BACKTRACE_LIB_TEST_MODE

void
function1() {
	ebacktrace();
}

void
function2() {
	function1();
}

void
function3() {
	function2();
}

void
function4() {
	function3();
}

int
main(int argc, const char *argv[]) {
	bfd_init();

	ebacktrace_init(argv[0]);

	function4();

	return 0;
}

#endif
