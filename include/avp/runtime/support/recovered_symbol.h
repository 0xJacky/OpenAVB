#ifndef AVP_RECOVERED_SYMBOL_H
#define AVP_RECOVERED_SYMBOL_H

/*
 * Formal AVPBooter mappings are independently callable ABI entries.  Keep
 * each mapped entry addressable even when the source call graph would let
 * the compiler inline it or the linker garbage-collect its section.
 */
#if defined(__ELF__)
#define AVP_RECOVERED_ENTRY __attribute__((used, retain, noinline))
#else
#define AVP_RECOVERED_ENTRY __attribute__((noinline))
#endif

#endif
