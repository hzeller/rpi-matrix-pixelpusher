CXXFLAGS=-Wall -O3 -g
OBJECTS=pixel-push.o
BINARIES=pixel-push

RGB_INCDIR=matrix/include
RGB_LIBDIR=matrix/lib
RGB_LIBRARY_NAME=rgbmatrix
RGB_LIBRARY=$(RGB_LIBDIR)/lib$(RGB_LIBRARY_NAME).a

PP_INCDIR=pp-server/include
PP_LIBDIR=pp-server/lib
PP_LIBRARY_NAME=pixel-push-server
PP_LIBRARY=$(PP_LIBDIR)/lib$(PP_LIBRARY_NAME).a

LDFLAGS+=-L$(PP_LIBDIR) -l$(PP_LIBRARY_NAME) \
         -L$(RGB_LIBDIR) -l$(RGB_LIBRARY_NAME) \
          -lrt -lm -lpthread

all : pixel-push

pixel-push : $(OBJECTS) $(RGB_LIBRARY) $(PP_LIBRARY)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)

$(RGB_LIBRARY): FORCE
	$(MAKE) -C $(RGB_LIBDIR)

$(PP_LIBRARY): FORCE
	$(MAKE) -C $(PP_LIBDIR)

pixel-push.o : pixel-push.cc

%.o : %.cc
	$(CXX) -I$(PP_INCDIR) -I$(RGB_INCDIR) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(BINARIES)
	$(MAKE) -C $(RGB_LIBDIR) clean
	$(MAKE) -C $(PP_LIBDIR) clean

FORCE:
.PHONY: FORCE
