D := $(CURDIR)

test.:
	ninja -C out/release all && ctest --test-dir out/release
	ninja -C out/debug all
	LD_PRELOAD=$(D)/out/release/libmimalloc.so.2 clang -DMI_BUILD_RELEASE -DMI_CMAKE_BUILD_TYPE=release -DMI_GIT_DESCRIBE=v2.2.4-41-gec91c5fc -DMI_MALLOC_OVERRIDE -DMI_SHARED_LIB -DMI_SHARED_LIB_EXPORT -Dmimalloc_EXPORTS -I$(D)/include -O3 -DNDEBUG -std=gnu11 -fPIC -Wall -Wextra -Wpedantic -Wno-unknown-pragmas -fvisibility=hidden -Wstrict-prototypes -Wno-static-in-inline -ftls-model=initial-exec -fno-builtin-malloc -o /tmp/t/alloc.c.o -c $(D)/src/alloc.c
