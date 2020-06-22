
rev=`support/config.guess`

all install config configure:
	@if [ -d ${rev} -a -f ${rev}/Makefile ]; then \
		echo "Configuration for ${rev} already exists"; \
		echo "Please \"cd ${rev}\" first"; \
	else \
		echo "Configuring ${rev}"; \
		mkdir -p ${rev}; \
		cd ${rev}; \
		sh ../support/configure ${CONFIGARGS}; \
		cd ..; \
		echo "Next cd ${rev}, edit config.h and run make to build"; \
	fi

clean:
	@echo 'To make clean move to the arch/OS specific directory'

distclean realclean: clean
	@echo "To make $@ remove all the arch/OS specific directories"

rcs:
	cii -H -R Makefile common doc include sasm sload support

#		/bin/cp ../support/Makefile.irc ../support/Makefile.ircd .; \
