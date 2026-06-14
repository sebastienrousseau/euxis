#include <stdio.h>

/* Minimal C fixture used by libs/parse tests. The expected named-
 * node shape: translation_unit { preproc_include, comment,
 * function_definition { ... } }. */

int add(int a, int b) {
    return a + b;
}

int main(void) {
    int sum = add(40, 2);
    printf("%d\n", sum);
    return 0;
}
