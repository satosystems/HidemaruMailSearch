#include <windows.h>

#ifdef __cpluspluc
extern "C" {
#endif
void *get_reg_value(const char *path, PDWORD type);

#ifdef __cpluscplus
}
#endif
