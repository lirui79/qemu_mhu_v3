#include "system.h"

#define MEMORY_TEST_ENABLE  0

/**
 * simple memory test
 */
#if (MEMORY_TEST_ENABLE == 1)
static int memory_check(uint32_t addr, char *prefix)
{
#define MEM_TEST_PATTERN 0xC001C0DE
    uint32_t val;
    val = READ32(addr);
    ts_printf("%s: 0x%x=0x%x\n", prefix, addr, val);
    WRITE32(addr, MEM_TEST_PATTERN);
    val = READ32(addr);
    ts_printf("%s: 0x%x=0x%x\n", prefix, addr, val);
    return (val == MEM_TEST_PATTERN) ? 0 : (-1);
}

static int memory_test(void)
{
    char *prefix;
    int   ret = 0;

    prefix = "TCM";
    ret |= memory_check(TCM_BASE, prefix);
    ts_printf("%s: RW test %s\n", prefix, (ret == 0) ? "OK" : "ERR");

    prefix = "DDR";
    ret |= memory_check(DDR_BASE, prefix);
    ts_printf("%s: RW test %s\n", prefix, (ret == 0) ? "OK" : "ERR");

    return ret;
}
#endif

/**
 * test entry
 */
int self_test(void)
{
    int ret = 0;
#if (MEMORY_TEST_ENABLE == 1)
    ret |= memory_test();
#endif
    ts_printf("SELF TEST %s\n", (ret == 0) ? "PASS" : "FAILED");
    return ret;
}
