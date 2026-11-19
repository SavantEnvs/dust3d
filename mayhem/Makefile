# Builds the Qt-free dust3d core (the sources application/application.pro lists under
# ../dust3d and ../third_party) into $(OBJDIR)/libdust3d.a. Driven by mayhem/build.sh,
# which passes SRCS (read from application.pro), OBJDIR and the flags. Incremental: a
# re-run only recompiles what changed (header deps via -MMD).
SRCS ?=
OBJDIR ?= mayhem/build/obj
XCXXFLAGS ?=
XCFLAGS ?=
CPPFLAGS_D3 := -D_USE_MATH_DEFINES -Ithird_party/autoremesher/tbbshim -I. -Ithird_party \
  -Ithird_party/rapidxml-1.13 -Ithird_party/earcut.hpp/include \
  -Ithird_party/autoremesher/include -Ithird_party/autoremesher/thirdparty/isotropicremesher \
  -Ithird_party/eigen
OBJS := $(patsubst %,$(OBJDIR)/%.o,$(SRCS))

$(OBJDIR)/libdust3d.a: $(OBJS)
	rm -f $@
	$(AR) rcs $@ $^

$(OBJDIR)/%.cc.o: %.cc
	@mkdir -p $(@D)
	$(CXX) -std=c++17 $(CPPFLAGS_D3) $(XCXXFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/%.cpp.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) -std=c++17 $(CPPFLAGS_D3) $(XCXXFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/%.c.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS_D3) $(XCFLAGS) -MMD -MP -c $< -o $@

-include $(OBJS:.o=.d)
