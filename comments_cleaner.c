#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<stdbool.h>
#include<ctype.h>

int main(int argc, char** argv) {
  if (argc < 2) {
    printf("Missing input file\n");
    return 0;
  }
  char* inFileName = argv[1];
  int len = strlen(inFileName);
  if (len < 3 || inFileName[len-1] != 'n'
        || inFileName[len-2] != 'i' || inFileName[len-3] != '.') {
    printf("The input file is not an 'in' extension file\n");
    return 0;
  }
  // Construct output filename.
  char* outFileName = (char*)malloc(sizeof(char)*(len+2));
  strcpy(outFileName, inFileName);
  outFileName[len-2] = 'o';
  outFileName[len-1] = 'u';
  outFileName[len] = 't';
  outFileName[len+1] = '\0';

  // Open files.
  FILE *infile = fopen(inFileName, "r");
  if (infile == NULL) {
    printf("The file doesn't exist\n");
    return 0;
  }

  // Buffers to reduce IO bottleneck, 
  // and for memory management.
  char buf[128];
  char outbuf[256];
  FILE *outfile = fopen(outFileName, "w");
  int i = 0;
  int outIdx = 0;

  // States to determine the current section of codes.
  bool isInMLComment = false;	// multi-line comments
  bool isInSLComment = false;	// single-line comments
  bool foundSlash = false;
  bool foundStar = false;
  bool isNewLineR = true;	// '/r' character
  bool isNewLineN = true;	// '/n' character
  bool skipChar = false;
  while (fgets(buf, sizeof(buf), infile) != NULL) {
    outIdx = 0;		// Reset outbuf's index
    for (i = 0; i < strlen(buf); i++) {
      if (isInMLComment) {
	// In multiline comment only terminate if found 
	// '*' then '/' right after.
        if (buf[i] == '*') {
          foundStar = true;
	}
	else if (foundStar && buf[i] == '/') {
          isInMLComment = false;
	}
	else {
          foundStar = false;
	}
      }
      else if (isInSLComment) {
	// In single line comment, terminate when
	// encounter new line character.
	// Special newline condition blank line.
	// Handle several type of newline characters.
        if (buf[i] == '\r') {
	  isInSLComment = false;
          if (!isNewLineR && !isNewLineN) {
            outbuf[outIdx] = '\r';
	    outIdx++;
	  }
	  isNewLineR = true;
	}
	else if (buf[i] == '\n') {
	  isInSLComment = false;
	  if (!isNewLineN) {
            outbuf[outIdx] = '\n';
	    outIdx++;
	  }
	  isNewLineN = true;
	}
      }
      else {
        if (buf[i] == '/') {
	  // Possible comment's slash
          if (foundSlash) {
            isInSLComment = true;
	    foundSlash = false;
	  }
	  else {
            foundSlash = true;
	  }
	}
	else if (foundSlash && buf[i] == '*') {
	  // Found multi line comment
          foundSlash = false;
	  isInMLComment = true;
	}
	else {
	  skipChar = false;
          if (foundSlash) {
	    // Not part of the comment, 
	    // so need to actually output
	    // the previous slash. 
            outbuf[outIdx] = '/';
	    outIdx++;
	    foundSlash = false;
	    isNewLineN = false;
	    isNewLineR = false;
	  }
	  if (buf[i] == '\r') {
            // Check for blank space.
            if (isNewLineR || isNewLineN) {
              skipChar = true;
	    }
	    isNewLineR = true;
	  } 
	  else if (buf[i] == '\n') {
	    // Check for blank space.
	    // Also handle "\r\n" sequences.
            if (isNewLineN) {
              skipChar = true;
	    }
	    isNewLineN = true;
	  }
	  else if (isspace(buf[i])) {
	    // Identify leading white space.
            if (isNewLineN || isNewLineR) {
              skipChar = true;
	    }
	  }
	  else {
	    isNewLineN = false;
	    isNewLineR = false;
	  }
	  if (!skipChar) {
            outbuf[outIdx] = buf[i];
	    outIdx++;
	  }
	}
      }
    }
    // Output into file.
    outbuf[outIdx] = '\0';
    outIdx++;
    fputs(outbuf, outfile);
  }
  //Handle the last slash.
  if (foundSlash) fputc('/', outfile);
  if (feof(infile)) {
    printf("Successfully delete all comments\n");
  }
  fclose(infile);
  fclose(outfile);
  return 0;
}
