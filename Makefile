all: p2g .presets

SRCS = $(wildcard *.cpp) $(wildcard pcb2gcode/*.cpp)
HDRS = $(wildcard *.hpp) $(wildcard pcb2gcode/*.hpp)
OBJECTS = $(patsubst %.cpp,%.o,$(SRCS))

CXXFLAGS += $(shell pkg-config --cflags opencv4 yaml-cpp)
LDFLAGS += $(shell pkg-config --libs opencv4 yaml-cpp)
LDFLAGS += -lpthread -lboost_system -lboost_filesystem

CFLAGS += -I$(PWD) -O3
CXXFLAGS += -I$(PWD) -O3 -std=c++20

p2g: $(OBJECTS)
	g++ -o $@ $(OBJECTS) $(LDFLAGS)

clean:
	$(RM) p2g $(OBJECTS) *.gcode
	make -C docker clean
	make -C presets clean

distclean: clean
	make -C docker distclean
	make -C presets clean

install: p2g
	install -m 755 p2g ~/.local/bin/p2g

install_html:
	mkdir -p ~/public_html/p2g 
	rm -rf ~/public_html/p2g
	cp -rL html/ ~/public_html/p2g/
	chmod a+rX,go-w -R ~/public_html/p2g/

.presets: presets/Makefile presets/frag/*
	make -C presets

docker/p2g.tar.xz: Makefile ${SRCS} ${HDRS} *.ts html/*
	tar -cJf docker/p2g.tar.xz Makefile ${SRCS} ${HDRS} *.ts presets html

rp2g: docker/p2g.tar.xz
	make -C docker build

rp2g-export: rp2g
	make -C docker export

rp2g-test: rp2g
	make -C docker run
