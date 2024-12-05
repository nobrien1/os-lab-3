// ACADEMIC INTEGRITY PLEDGE
//
// - I have not used source code obtained from another student nor
//   any other unauthorized source, either modified or unmodified.
//
// - All source code and documentation used in my program is either
//   my original work or was derived by me from the source code
//   published in the textbook for this course or presented in
//   class.
//
// - I have not discussed coding details about this project with
//   anyone other than my instructor. I understand that I may discuss
//   the concepts of this program with other students and that another
//   student may help me debug my program so long as neither of us
//   writes anything during the discussion or modifies any computer
//   file during the discussion.
//
// - I have violated neither the spirit nor letter of these restrictions.
//
//
//
// Signed: Nathaniel O'Brien    Date: 12/3/2024

//filesys.c
//Based on a program by Michael Black, 2007
//Revised 11.3.2020 O'Neil

#include <stdio.h>
#include <ctype.h>
#include <string.h>

// Define constants
#define SECTOR_SIZE 512
#define NUM_SECTORS 512
#define SECTORS_TRACKED 511
#define MAX_FILE_SIZE 12288
#define DIR_SECTOR 257
#define MAP_SECTOR 256

// Define function prototypes
void listFiles(char dir[SECTOR_SIZE], char map[NUM_SECTORS]);
void printFile(char *fileName, char dir[SECTOR_SIZE], char map[NUM_SECTORS], FILE *floppy);
void createFile(char *filename, char dir[SECTOR_SIZE], char map[NUM_SECTORS], FILE *floppy);
void deleteFile(char *filename, char dir[SECTOR_SIZE], char map[NUM_SECTORS], FILE *floppy);
int getDirIndex(char *filename, char dir[SECTOR_SIZE]);

int main(int argc, char* argv[])
{
	int i, j, size, noSecs, startPos;

	//open the floppy image
	FILE* floppy;
	floppy = fopen("floppya.img", "r+");
	if (floppy==0)
	{
		printf("ERROR: floppya.img not found, please add file floppya.img to the same directory as the executable\n");
		return 0;
	}

	//load the disk map from sector 256
	char map[NUM_SECTORS];
	fseek(floppy, SECTOR_SIZE * 256, SEEK_SET);
	for (i = 0; i < SECTOR_SIZE; i++)
		map[i]=fgetc(floppy);

	//load the directory from sector 257
	char dir[SECTOR_SIZE];
	fseek(floppy, SECTOR_SIZE * 257, SEEK_SET);
	for (i = 0; i < SECTOR_SIZE; i++)
		dir[i]=fgetc(floppy);

	// Process command if at least 1 more argument was passed
	if (argc > 1)
	{
		switch (argv[1][0])
		{
		case 'D':
			// Delete file
			if (argc >= 3)
			{
				deleteFile(argv[2], dir, map, floppy);
			}
			else
			{
				printf("ERROR: No file name passed to the program, please pass a file name.\n");
			}
			break;
		case 'L':
			// List files
			listFiles(dir, map);
			break;
		case 'M':
			// Create file
			if (argc >= 3)
			{
				createFile(argv[2], dir, map, floppy);
			}
			else
			{
				printf("ERROR: No file name passed to the program, please pass a file name.\n");
			}
			break;
		case 'P':
			// Print file
			if (argc >= 3)
			{
				printFile(argv[2], dir, map, floppy);
			}
			else
			{
				printf("ERROR: No file name passed to the program, please pass a file name.\n");
			}
			break;
		default:
			printf("ERROR: Invalid argument, command does not exist for given argument.\n");
			break;
		}
	}
	else
	{
		printf("ERROR: No arguments passed to the program, please pass arguments.\n");
	}

	fclose(floppy);
}

// List files and sizes in bytes
void listFiles(char dir[SECTOR_SIZE], char map[NUM_SECTORS])
{
	int i, j, sectorsUsed = 0;

	// Split into 16 byte entries
	// first 8 bytes are the file name
	// next byte is the file type
	// next byte is the starting sector index
	// next byte is the length in sectors

	// Loop through all entries in the directory
	for (i = 0; i < SECTOR_SIZE; i = i + 16)
	{
		if (dir[i] == 0)
			continue;

		// Print the file name (display all characters even tabs)
		for (j = 0; j < 8; j++)
		{
			if (dir[i + j] != 0)
				printf("%c", dir[i + j]);
		}

		// Print the file extension
		printf(".");
		if (dir[i + 8] != 0)
			printf("%c", dir[i + 8]); // File extension at index 8 of the dir entry

		// Print the file size
		printf("\t%6d bytes\n", SECTOR_SIZE * dir[i + 10]);
	}

	for (i = 0; i < NUM_SECTORS; i++)
	{
		if (map[i] == -1)
			sectorsUsed++;
	}

	printf("-----------------------------------------\n");
	printf("%d bytes used; %d bytes available\n", SECTOR_SIZE * sectorsUsed, SECTOR_SIZE * (SECTORS_TRACKED - sectorsUsed));
}

// Print the contents of a file
void printFile(char *filename, char dir[SECTOR_SIZE], char map[NUM_SECTORS], FILE *floppy)
{
	int i, j, dirIndex, start, length;
	char buffer[MAX_FILE_SIZE];

	// Check if the filename is too long
	if (strlen(filename) > 8)
	{
		printf("ERROR: File name is too long, please enter a file name with a maximum of 8 characters\n");
		return;
	}

	// Get the index of the file in the dir
	dirIndex = getDirIndex(filename, dir);

	// Check if the file exists
	if (dirIndex < 0)
	{
		printf("ERROR: File not found, please make sure you typed the correct file name\n");
		return;
	}

	// Check if the file is a text file
	if (dir[dirIndex + 8] != 't')
	{
		printf("ERROR: File must be a text file to be displayed, please enter a file with extension \".t\"\n");
		return;
	}

	start = dir[dirIndex + 9];	 // Start sector is at index 9 of the dir entry
	length = dir[dirIndex + 10]; // Length in sectors is at index 10 of the dir entry

	// Load the file into the buffer
	fseek(floppy, SECTOR_SIZE * start, SEEK_SET);
	for (i = 0; i < length * SECTOR_SIZE; i++)
	{
		buffer[i] = fgetc(floppy);
	}

	// Print the buffer until we hit an empty character
	for (i = 0; i < length * SECTOR_SIZE; i++)
	{
		if (buffer[i] == 0)
			return;

		printf("%c", buffer[i]);
	}
}

// Creates a file and writes the contents to the floppy
void createFile(char *filename, char dir[SECTOR_SIZE], char map[NUM_SECTORS], FILE *floppy)
{
	int i, j, found, freeDir = -1, freeSector = -1;
	char content[SECTOR_SIZE];

	// If file with the same name already exists, return an error
	if (getDirIndex(filename, dir) >= 0)
	{
		printf("ERROR: A file with this name already exists, please enter a different file name\n");
		return;
	}

	// Check if the filename is too long
	if (strlen(filename) > 8)
	{
		printf("ERROR: File name is too long, please enter a file name with a maximum of 8 characters\n");
		return;
	}

	// Loop through all dir entries until we find an empty one
	for (i = 0; i < SECTOR_SIZE; i += 16)
	{
		if (dir[i] == 0 && freeDir == -1)
		{
			freeDir = i;
		}
	}

	// Check if a free dir exists
	if (freeDir == -1)
	{
		printf("ERROR: Not enough space for a new file - no more directories available\n");
		return;
	}

	// Find a free sector
	for (i = 0; i < SECTOR_SIZE; i++)
	{
		if (map[i] == 0)
		{
			freeSector = i;
			break;
		}
	}

	// Check if a free sector exists
	if (freeSector == -1)
	{
		printf("ERROR: Not enough space for a new file - no more sectors available\n");
		return;
	}

	// User inputs the contents
	printf("Enter file content (max 512 bytes, press CTRL + C to cancel): ");
	fgets(content, SECTOR_SIZE, stdin);

	// Check if the content is too large
	if (strlen(content) > SECTOR_SIZE)
	{
		printf("ERROR: File content is too large, please enter a file content with a maximum of 512 characters\n");
		return;
	}

	// Update dir
	for (j = 0; j < 8; j++)
	{
		dir[freeDir + j] = (j < strlen(filename)) ? filename[j] : 0;
	}
	dir[freeDir + 8] = 't';		   // Set file type to text at the 8th byte of the dir
	dir[freeDir + 9] = freeSector; // Set start sector index at the 9th byte of the dir
	dir[freeDir + 10] = 1;		   // Set length in sectors at the 10th byte of the dir

	// Mark the sector as used in the map
	map[freeSector] = -1;

	// Write file content to the floppy in the selected sector
	fseek(floppy, SECTOR_SIZE * freeSector, SEEK_SET);
	for (i = 0; i < SECTOR_SIZE; i++)
		fputc(content[i], floppy);

	// Write the map back to the floppy image
	fseek(floppy, SECTOR_SIZE * 256, SEEK_SET);
	for (i = 0; i < SECTOR_SIZE; i++)
		fputc(map[i], floppy);

	// Write the dir back to the floppy image
	fseek(floppy, SECTOR_SIZE * 257, SEEK_SET);
	for (i = 0; i < SECTOR_SIZE; i++)
		fputc(dir[i], floppy);
}

// Delete the file with the matching filename (doesn't delete data just dir and map entries)
void deleteFile(char *filename, char dir[SECTOR_SIZE], char map[NUM_SECTORS], FILE *floppy)
{
	int i, j, start, length, dirIndex;
	char input[8];

	// Check if the filename is too long
	if (strlen(filename) > 8)
	{
		printf("ERROR: File name is too long, please enter a file name with a maximum of 8 characters\n");
		return;
	}

	// Get the index of the file in the dir
	dirIndex = getDirIndex(filename, dir);

	// Check if the file exists
	if (dirIndex < 0)
	{
		printf("ERROR: File not found, please make sure you typed the filename correctly\n");
		return;
	}

	// Confirm deletion
	printf("Are you sure you want to delete the file? (Y/n): ");
	fgets(input, 8, stdin);

	if (input[0] != 'Y' && input[0] != 'y')
	{
		return;
	}

	start = dir[dirIndex + 9];	 // Start sector is at index 9 of the dir entry
	length = dir[dirIndex + 10]; // Length in sectors is at index 10 of the dir entry

	// Clear sectors in the map
	for (i = 0; i < length; i++)
	{
		map[start + i] = 0;
	}

	// Clear entries in the dir
	for (i = 0; i < 16; i++)
	{
		dir[dirIndex + i] = 0;
	}

	// Write the map back to the floppy image
	fseek(floppy, SECTOR_SIZE * 256, SEEK_SET);
	for (i = 0; i < SECTOR_SIZE; i++)
		fputc(map[i], floppy);

	// Write the dir back to the floppy image
	fseek(floppy, SECTOR_SIZE * 257, SEEK_SET);
	for (i = 0; i < SECTOR_SIZE; i++)
		fputc(dir[i], floppy);

	printf("File \"%s\" deleted successfully\n", filename);
}

// Get the index of the file with the filename in the passed dir
int getDirIndex(char *filename, char dir[SECTOR_SIZE])
{
	int i, j, found;

	// Loop through all entries in the dir
	for (i = 0; i < SECTOR_SIZE; i = i + 16)
	{
		found = 1;

		// Check if filenames match
		for (j = 0; j < 8; j++)
		{
			if (filename[j] == 0 && dir[i + j] == 0)
				break;
			if (filename[j] != dir[i + j])
			{
				found = 0;
				break;
			}
		}

		if (found)
		{
			// If found, return the index
			return i;
		}
	}

	// If not found, return -1
	return -1;
}