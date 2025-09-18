//gcc *.c -o mp3_editor
//./mp3_editor -v sample.mp3
//./mp3_editor -e -t "Rocky bhai" sample.mp3
//./mp3_editor -e -a "Yash" sample.mp3
//./mp3_editor -e -A "KGF" sample.mp3
//./mp3_editor -e -y 2025 sample.mp3
//./mp3_editor -e -m "kannada" sample.mp3
//./mp3_editor -e -c "Remastered Version" sample.mp3
//./mp3_editor --help

#include <stdio.h>
#include <string.h>
#include "mp3_tag.h"

int main(int argc, char *argv[]) 
{
    
    if (argc < 2) 
    {
        show_help();
        return 1;
    }

    
    if (strcmp(argv[1], "--help") == 0) 
    {
        show_help();
    }
    
    else if (strcmp(argv[1], "-v") == 0 && argc == 3) 
    {
        read_tags(argv[2]); 
    }
    
    else if (strcmp(argv[1], "-e") == 0) 
    {
        edit_tags(argc, argv); 
    }
   
    else 
    {
        printf("Invalid command.\n");
        show_help(); 
    }

    return 0; 
}


