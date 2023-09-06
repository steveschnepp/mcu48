ROMS = $(wildcard *.rom)
CROMS = $(ROMS:.rom=.c)

TOPTARGETS := all clean

all: $(CROMS)

clean:
	rm -f $(CROMS)

%.c: %.rom
	srec_cat $< -binary -o $@ -c-array hp48_rom
