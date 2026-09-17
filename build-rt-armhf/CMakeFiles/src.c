#if !(__ARM_FP & 0x8)
                                   #error No double-precision support!
                                   #endif
                                   int main(void) { return 0; }
