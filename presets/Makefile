MACHINES := CNC3018 BUNGARD
PRESETS := basic full voronoi-basic voronoi-full voronoi-multitool

TARGETS=$(foreach m, $(MACHINES), $(foreach p, $(PRESETS), $m-$p.p2g ) )

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

CNC3018-%.p2g: frag/00-head.p2g frag/01-tools-CNC3018.p2g frag/02-jobs-CNC3018-%.p2g frag/03-jobs-CNC3018-common.p2g frag/04-outputs-CNC3018.p2g
	cat >$@ $^

BUNGARD-%.p2g: frag/00-head.p2g frag/01-tools-BUNGARD.p2g frag/02-jobs-BUNGARD-%.p2g frag/03-jobs-BUNGARD-common.p2g frag/04-outputs-BUNGARD.p2g
	cat >$@ $^
