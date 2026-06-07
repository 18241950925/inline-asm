void hpu_mm_complete(void) {
    __asm__ volatile(
        "pmodld p4, 0, 0 \n\t"
        "pmul p2, p0, p1 \n\t"
        "psync 0, 0 \n\t"
        :
        :
        : "memory"
    );
}
