#include <loaf.h>

int main() {
    for (;;)
        sys_yield_hlt();
    
    return 0;
}