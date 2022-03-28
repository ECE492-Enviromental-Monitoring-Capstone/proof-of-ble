EXE=a.out

${EXE}: main.c
	cc main.c -o ${EXE}

.PHONY: run clean

run: ${EXE}
	./${EXE}

clean:
	rm -rf ${EXE}