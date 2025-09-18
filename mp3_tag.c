#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mp3_tag.h"

#define MAX_FRAMES 6

// Supported ID3v2.3 frame identifiers and their labels
const char *frame_ids[MAX_FRAMES]   = {"TIT2", "TPE1", "TALB", "TYER", "TCON", "COMM"};
const char *frame_labels[MAX_FRAMES] = {"TITLE", "ARTIST", "ALBUM", "YEAR", "GENRE", "COMMENT"};

// Function to read and display MP3 tag values
int read_tags(const char *filename) 
{
    FILE *fp = fopen(filename, "rb"); // Open file in binary read mode
    if (!fp) 
    {
        perror("Cannot open file");
        return 1;
    }

    char header[10];
    fread(header, 1, 10, fp); // Read ID3v2.3 header

    // Check if it's a valid ID3v2.3 tag
    if (strncmp(header, "ID3", 3) != 0 || header[3] != 3)
    {
        printf("Not a valid ID3v2.3 file.\n");
        fclose(fp);
        return 1;
    }

    // Decode syncsafe integer for tag size
    int tag_size = ((header[6] & 0x7F) << 21) |
                   ((header[7] & 0x7F) << 14) |
                   ((header[8] & 0x7F) << 7)  |
                   (header[9] & 0x7F);
    long tag_end = tag_size + 10;

    char *latest_data[MAX_FRAMES] = {NULL};
    int latest_sizes[MAX_FRAMES] = {0};

    // Loop through frames
    while (!feof(fp)) 
    {
        long pos = ftell(fp);
        if (pos >= tag_end && pos > 1e6) 
        break;

        char id[5] = {0};
        if (fread(id, 1, 4, fp) != 4 || id[0] == 0) 
        break;

        unsigned char size_bytes[4];
        if (fread(size_bytes, 1, 4, fp) != 4) 
        break;

        int size = (size_bytes[0] << 24) | (size_bytes[1] << 16) |
                   (size_bytes[2] << 8) | size_bytes[3];
        if (size <= 0 || size > 5000) 
        break;

        fseek(fp, 2, SEEK_CUR); // Skip flags

        char *data = malloc(size);
        if (!data) 
        break;

        if (fread(data, 1, size, fp) != size) 
        {
            free(data);
            break;
        }

        // Store latest value if matching frame
        for (int i = 0; i < MAX_FRAMES; i++) 
        {
            if (strncmp(id, frame_ids[i], 4) == 0) 
            {
                free(latest_data[i]); // Free old if any
                latest_data[i] = data;
                latest_sizes[i] = size;
                data = NULL;
                break;
            }
        }

        free(data); // Free only if not stored
    }

    // Print collected tags
    printf("\n---- MP3 TAG DETAILS ----\n");
    for (int i = 0; i < MAX_FRAMES; i++) 
    {
        if (latest_data[i]) {
            // Print skipping encoding byte (first byte)
            printf("%-8s: %.*s\n", frame_labels[i], latest_sizes[i] - 1, latest_data[i] + 1);
            free(latest_data[i]);
        } 
        else 
        {
            printf("%-8s: \n", frame_labels[i]);
        }
    }

    fclose(fp);
    return 0;
}

// Function to update a tag frame or append a new one if needed
void update_or_append_frame(FILE *fp, const char *frame_id, const char *text) 
{
    fseek(fp, 10, SEEK_SET); // Skip header
    long pos;
    int found = 0;

    // Search for existing frame
    while (!feof(fp)) 
    {
        pos = ftell(fp);

        char id[5] = {0};
        fread(id, 1, 4, fp);
        if (id[0] == 0 || feof(fp)) break;

        unsigned char size_bytes[4];
        fread(size_bytes, 1, 4, fp);
        int size = (size_bytes[0] << 24) | (size_bytes[1] << 16) |
                   (size_bytes[2] << 8) | size_bytes[3];

        fseek(fp, 2, SEEK_CUR); // Skip flags

        if (strncmp(id, frame_id, 4) == 0) 
        {
            // If new text fits, overwrite
            if (strlen(text) + 1 <= size) 
            {
                fseek(fp, pos, SEEK_SET);
                fwrite(frame_id, 1, 4, fp);     // ID
                fwrite(size_bytes, 1, 4, fp);   // size
                fwrite("\0\0", 1, 2, fp);       // flags
                fputc(0x00, fp);                // encoding
                fwrite(text, 1, strlen(text), fp); // actual text

                int padding = size - 1 - strlen(text);
                for (int i = 0; i < padding; i++) fputc('\0', fp);

                printf("Updated %s to \"%s\"\n", frame_id, text);
                found = 1;
            } else 
            {
                printf("Existing %s frame too small, will append new frame...\n", frame_id);
            }
            break;
        }

        fseek(fp, size, SEEK_CUR); // move to next frame
    }

    // If frame not found or too small, append new one
    if (!found) {
        fseek(fp, 0, SEEK_END);
        int text_len = strlen(text);
        int total_size = text_len + 1;

        fwrite(frame_id, 1, 4, fp);
        unsigned char size_bytes[4] = 
        {
            (total_size >> 24) & 0xFF,
            (total_size >> 16) & 0xFF,
            (total_size >> 8) & 0xFF,
            total_size & 0xFF
        };
        fwrite(size_bytes, 1, 4, fp);  // frame size
        fwrite("\0\0", 1, 2, fp);      // flags
        fputc(0x00, fp);              // encoding
        fwrite(text, 1, text_len, fp); // actual text

        printf("Appended new %s frame with \"%s\"\n", frame_id, text);
    }
}

// Function to handle -e (edit) option and update tags
int edit_tags(int argc, char *argv[]) 
{
    const char *filename = argv[argc - 1]; // Last argument is the file name
    FILE *fp = fopen(filename, "rb+");
    if (!fp) {
        perror("Cannot open file");
        return 1;
    }

    char header[10];
    fread(header, 1, 10, fp);

    // Validate ID3v2.3 header
    if (strncmp(header, "ID3", 3) != 0 || header[3] != 3) 
    {
        printf("Invalid or unsupported tag.\n");
        fclose(fp);
        return 1;
    }

    // Loop through edit options like -a, -t, -y, etc.
    for (int i = 2; i < argc - 1; i++) 
    {
        const char *frame = NULL;
        if (strcmp(argv[i], "-t") == 0) frame = "TIT2";
        else if (strcmp(argv[i], "-a") == 0) frame = "TPE1";
        else if (strcmp(argv[i], "-A") == 0) frame = "TALB";
        else if (strcmp(argv[i], "-y") == 0) frame = "TYER";
        else if (strcmp(argv[i], "-m") == 0) frame = "TCON";
        else if (strcmp(argv[i], "-c") == 0) frame = "COMM";

        // Apply change if valid and has value
        if (frame && argv[i + 1]) {
            update_or_append_frame(fp, frame, argv[i + 1]);
            i++; // skip value
        }
    }

    fclose(fp);
    return 0;
}

// Display usage instructions
void show_help() 
{
    printf("\nUsage:\n");
    printf("  ./mp3_editor -v <file.mp3>              # View tags\n");
    printf("  ./mp3_editor -e -a \"Artist\" file.mp3     # Edit Artist\n");
    printf("  ./mp3_editor -e -t \"Title\" file.mp3      # Edit Title\n");
    printf("  ./mp3_editor -e -y \"2025\" file.mp3       # Edit Year\n");
    printf("  Tags: -t=title -a=artist -A=album -y=year -m=genre -c=comment\n");
    printf("  ./mp3_editor --help                     # Show help\n\n");
}
