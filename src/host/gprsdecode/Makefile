default:
	@echo [CC] $@
	@gcc -Wall -O3 -pipe -flto -std=gnu99 -g -o gprsdecode main.c gprs.c output.c rlcmac.c crc.c -pthread -losmogsm -losmocore

clean:
	rm gprsdecode

check: default
	@echo test CS2
	@./gprsdecode -f gprs_262_80_0001_0000_20110710_2251_875_494777_0f.sample > cs2.dat
	@diff gprs_262_80_0001_0000_20110710_2251_875_494777_0f.decoded cs2.dat
	@echo test CS3
	@./gprsdecode -f gprs_20160129_1744_878_16846_0c.sample > cs3.dat
	@diff gprs_20160129_1744_878_16846_0c.decoded cs3.dat

