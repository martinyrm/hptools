#ifndef	_GETOPT_H_
#define	_GETOPT_H_

#define	WRITE(s)	write(2, argv[0], strlen(argv[0])); \
						write(2, s, strlen(s)); \
						write(2, errbuf, 2);

extern int	optopt, opterr, optind;
extern char	*optarg;


extern int getopt( int , char * const argv[], const char *);

#endif
