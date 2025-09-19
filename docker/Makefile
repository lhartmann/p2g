D=podman

all:
	echo "Targets: build run"

run: .build
	${D} run --rm -ti --name rp2g-server -p 8644:8644 rp2g-server

build: .build

.build: Dockerfile entrypoint.sh
	${D} build . -t rp2g-server
	touch .build

clean:
	rm -f .build

distclean: clean
	${D} image rm rp2g-server || true
	${D} image prune --external -f
