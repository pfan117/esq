#ifndef __HMT_TEST_FUNCTION_HEADERS_INCLUDED__
#define __HMT_TEST_FUNCTION_HEADERS_INCLUDED__

typedef int (*tt)(void);

extern int test_driver(
		int max, int scale,
		char ** patterns, void ** return_values, int cnt,
		char ** strings, void ** espected_values, int result_cnt);

extern int test_function_000(void);
extern int test_function_001(void);
extern int test_function_002(void);
extern int test_function_003(void);
extern int test_function_004(void);
extern int test_function_005(void);
extern int test_function_006(void);
extern int test_function_007(void);
extern int test_function_008(void);
extern int test_function_009(void);
extern int test_function_010(void);
extern int test_function_011(void);
extern int test_function_012(void);
extern int test_function_013(void);

#endif
