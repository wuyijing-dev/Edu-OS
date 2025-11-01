# ===========================================================================
# EduOS User Space - Master Makefile
# ===========================================================================

.PHONY: all libc libgui apps clean

all: libc libgui apps

libc:
	@echo "Building libc..."
	$(MAKE) -C libc
	@echo ""

libgui: libc
	@echo "Building libgui..."
	$(MAKE) -C libgui
	@echo ""

apps: libc libgui
	@echo "Building applications..."
	$(MAKE) -C apps
	@echo ""

clean:
	@echo "Cleaning user space..."
	$(MAKE) -C libc clean
	$(MAKE) -C libgui clean
	$(MAKE) -C apps clean
	rm -rf build
	@echo "✓ Clean complete"

info:
	@echo "EduOS User Space Build System"
	@echo "============================="
	@echo "Structure:"
	@echo "  libc/    - Standard C Library"
	@echo "  libgui/  - GUI Library"
	@echo "  apps/    - Applications"
	@echo ""
	@echo "Targets:"
	@echo "  make          - Build everything"
	@echo "  make libc     - Build libc only"
	@echo "  make libgui   - Build libgui only"
	@echo "  make apps     - Build apps only"
	@echo "  make clean    - Clean all"

