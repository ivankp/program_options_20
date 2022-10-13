.PHONY: all clean

ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean))) #############

CFLAGS := -Wall -O3 -flto
# CFLAGS := -Wall -Og -g
CFLAGS += -fmax-errors=3 -Iinclude

# generate .d files during compilation
DEPFLAGS = -MT $@ -MMD -MP -MF .build/$*.d

FIND_MAIN := \
  find src -type f -regex '.*\.c+$$' \
  | xargs grep -l '^\s*int\s\+main\s*(' \
  | sed 's:^src/\(.*\)\.c\+$$:bin/\1:'
EXE := $(shell $(FIND_MAIN))

all: $(EXE)

.PRECIOUS: .build/%.o

bin/%: .build/%.o
	@mkdir -pv $(dir $@)
	$(CXX) $(LDFLAGS) $(filter %.o,$^) -o $@ $(LDLIBS)

.build/%.o: src/%.cc
	@mkdir -pv $(dir $@)
	$(CXX) -std=c++20 $(CFLAGS) $(DEPFLAGS) -c $(filter %.cc,$^) -o $@

-include $(shell [ -d '.build' ] && find .build -type f -name '*.d')

endif ###############################################################

clean:
	@rm -frv .build bin

