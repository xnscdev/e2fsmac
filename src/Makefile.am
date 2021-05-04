KEXT_BUNDLE = $(KEXT_NAME).kext

AM_CPPFLAGS = -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT \
	-DNO_XCODE -DKEXT_NAME=\"$(KEXT_NAME)\" -DKEXT_BUNDLE=$(KEXT_BUNDLE) \
	-DKEXT_VERSION=\"$(KEXT_VERSION)\" -DKEXT_BUILD=$(KEXT_BUILD) \
	-I$(sdkdir)/System/Library/Frameworks/Kernel.framework/Headers \
	-I$(sdkdir)/System/Library/Frameworks/Kernel.framework/PrivateHeaders \
	-nostdinc
AM_CFLAGS = -std=gnu11 -fno-builtin -fno-common -mkernel -Wall \
	-Werror=implicit -Wno-zero-length-array

KEXTLIBSFLAGS = -xml -unsupported -undef-symbols

noinst_PROGRAMS = e2fsmac

e2fsmac_SOURCES =	\
	init.c

e2fsmac_LDFLAGS = -mmacosx-version-min=$(sdkversion) -nostdlib -Wl,-kext \
	-Wl,-export_dynamic
e2fsmac_LDADD = -lkmod -lcc_kext

$(KEXT_BUNDLE): e2fsmac Info.plist
	$(MKDIR_P) $@/Contents/MacOS
	$(INSTALL) -m 755 $< $@/Contents/MacOS/$<
	$(SED) -e 's/KEXTLIBS//g' Info.plist > $@/Contents/Info.plist
	$(AWK) '/KEXTLIBS/{system("$(KEXTLIBS) $(KEXTLIBSFLAGS) $@");next};1' \
	    Info.plist > $@/Contents/Info.plist~
	mv $@/Contents/Info.plist~ $@/Contents/Info.plist

# This will make the default target build the kext bundle

BUILT_SOURCES = $(KEXT_BUNDLE)

.PHONY: clean-kext
clean-kext:
	-rm -r -f $(KEXT_BUNDLE)
clean-local: clean-kext

install-exec-hook: $(KEXT_BUNDLE)
	cp -r $< $(DESTDIR)$(kextdir)

uninstall-hook:
	-rm -r -f $(DESTDIR)$(kextdir)/$(KEXT_BUNDLE)
