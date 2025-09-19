D=podman

IMAGE=$(HOME)/Documentos/public_html/rpg2-server-docker.xz

all:
	echo "Targets: build run export"

run: .build
	${D} run --rm -ti --name rp2g-server -p 8644:8644 rp2g-server

build: .build

.build: Dockerfile p2g.tar.xz entrypoint.sh
	${D} build . -t rp2g-server
	touch .build

clean:
	rm -f p2g.tar.xz .build

distclean: clean
	${D} image rm rp2g-server || true
	${D} image prune --external -f

export: ${IMAGE}

$(IMAGE): .build
	${D} image save rp2g-server | xz -T${shell nproc} > ${IMAGE}
