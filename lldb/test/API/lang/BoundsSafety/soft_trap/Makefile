# FIXME: mockSoftTrapRuntime.c shouldn't really be built with -fbounds-safety
C_SOURCES := main.c mockSoftTrapRuntime.c

soft-trap-test-minimal: CFLAGS_EXTRAS := -fbounds-safety -fbounds-safety-soft-traps=call-minimal
soft-trap-test-minimal: all

soft-trap-test-with-str: CFLAGS_EXTRAS := -fbounds-safety -fbounds-safety-soft-traps=call-with-str
soft-trap-test-with-str: all

a.out: lib_dylib_with_soft_trap

include Makefile.rules

lib_dylib_with_soft_trap:
	"$(MAKE)" -f $(MAKEFILE_RULES) \
		DYLIB_ONLY=YES DYLIB_C_SOURCES=dylib.c DYLIB_NAME=dylib_with_soft_trap \
		CFLAGS_EXTRAS="$(CFLAGS_EXTRAS)"
