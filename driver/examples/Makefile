LDFLAGS=-I ../kernel

all: sonic_cap sonic_rpt sonic_crpt

sonic_user: sonic_user.c ../kernel/stat.c
	$(CC) -o $@ -DSONIC_TWO_PORTS $^ $(LDFLAGS)

sonic_cap: sonic_cap.c

sonic_rpt: sonic_rpt.c

sonic_crpt: sonic_crpt.c

clean: 
	rm -f sonic_user sonic_cap sonic_rpt sonic_crpt
