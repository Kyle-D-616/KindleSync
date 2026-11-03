#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>


int main (int argc, char **argv) {
	if (argc < 2){
		fprintf(stderr, "Error: Missing arguments.\n   Usage: %s <booksfolder>\n", argv[0]);
	}
	else if (argc > 2){
		fprintf(stderr,"Error: Too many argumetns.\n   Did you wrap you'r path in quotes?\n");
	}
	else {
		opendir(argv[1]);
	}
	return 0;
}
