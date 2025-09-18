

#ifndef MP3_TAG_H
#define MP3_TAG_H


int read_tags(const char *filename);
int edit_tags(int argc, char *argv[]);
void show_help(void);
#endif  
