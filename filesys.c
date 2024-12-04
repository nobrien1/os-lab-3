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

// Define constants
#define SECTOR_SIZE 512
#define MAX_FILE_SIZE 12288
#define DIR_SECTOR 257
#define MAP_SECTOR 256

// Define function prototypes
void listFiles(char dir[SECTOR_SIZE]);
void printFile(char *fileName, char dir[SECTOR_SIZE], char map[SECTOR_SIZE], FILE *floppy);
void createFile(char *filename, char dir[SECTOR_SIZE], char map[SECTOR_SIZE], FILE *floppy);
void deleteFile(char *filename, char dir[SECTOR_SIZE], char map[SECTOR_SIZE], FILE *floppy);
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
	char map[SECTOR_SIZE];
	fseek(floppy, SECTOR_SIZE * 256, SEEK_SET);
	for (i = 0; i < SECTOR_SIZE; i++)
		map[i]=fgetc(floppy);

	//load the directory from sector 257
	char dir[SECTOR_SIZE];
	fseek(floppy, SECTOR_SIZE * 257, SEEK_SET);
	for (i = 0; i < SECTOR_SIZE; i++)
		dir[i]=fgetc(floppy);

	// print disk map
	printf("Disk usage map:\n");
	printf("      0 1 2 3 4 5 6 7 8 9 A B C D E F\n");
	printf("     --------------------------------\n");
	for (i=0; i<16; i++) {
		switch(i) {
			case 15: printf("0xF_ "); break;
			case 14: printf("0xE_ "); break;
			case 13: printf("0xD_ "); break;
			case 12: printf("0xC_ "); break;
			case 11: printf("0xB_ "); break;
			case 10: printf("0xA_ "); break;
			default: printf("0x%d_ ", i); break;
		}
		for (j=0; j<16; j++) {
			if (map[16*i+j]==-1) printf(" X"); else printf(" .");
		}
		printf("\n");
	}

	if (argc > 1)
	{
		switch (argv[1][0])
		{
		case 'D':
			// delete file
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
			// list files
			listFiles(dir);
			break;
		case 'M':
			// create file
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
			// print file
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
void listFiles(char dir[SECTOR_SIZE])
{
	int i, j;

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

		// Print the file name
		for (j = 0; j < 8; j++)
		{
			if (!isspace(dir[i + j]))
				printf("%c", dir[i + j]);
		}
		printf(".");
		// Print the file extension
		if (!isspace(dir[i + 8]))
			printf("%c", dir[i + 8]);

		// Print the file size
		printf("\t%6d bytes\n", SECTOR_SIZE * dir[i + 10]);
	}
}

// Print the contents of a file
void printFile(char *filename, char dir[SECTOR_SIZE], char map[SECTOR_SIZE], FILE *floppy)
{
	int i, j, dirIndex, start, length;
	char buffer[MAX_FILE_SIZE];

	if (strlen(filename) > 8)
	{
		printf("ERROR: File name is too long, please enter a file name with a maximum of 8 characters\n");
		return;
	}

	dirIndex = getDirIndex(filename, dir);

	if (dirIndex < 0)
	{
		printf("ERROR: File not found, please make sure you typed the correct file name\n");
		return;
	}

	if (dir[dirIndex + 8] != 't')
	{
		printf("ERROR: File must be a text file to be displayed, please enter a file with extension \".t\"\n");
		return;
	}

	start = dir[dirIndex + 9];
	length = dir[dirIndex + 10];

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
void createFile(char *filename, char dir[SECTOR_SIZE], char map[SECTOR_SIZE], FILE *floppy)
{
	int i, j, found, freeDir = -1, freeSector = -1;
	char content[SECTOR_SIZE];

	// If file with the same name already exists, return an error
	if (getDirIndex(filename, dir) >= 0)
	{
		printf("ERROR: A file with this name already exists, please enter a different file name\n");
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

	if (freeDir == -1)
	{
		printf("ERROR: Not enough space for a new file - no more directories available\n");
		return;
	}

	// Find a free sector and assign it's index to freeSector
	for (i = 0; i < SECTOR_SIZE; i++)
	{
		if (map[i] == 0)
		{
			freeSector = i;
			break;
		}
	}

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

	//write the map and directory back to the floppy image
	fseek(floppy, SECTOR_SIZE * 256, SEEK_SET);
	for (i = 0; i < SECTOR_SIZE; i++)
		fputc(map[i], floppy);

	fseek(floppy, SECTOR_SIZE * 257, SEEK_SET);
	for (i = 0; i < SECTOR_SIZE; i++)
		fputc(dir[i], floppy);
}

// Delete the file with the matching filename (doesn't delete data just dir and map entries)
void deleteFile(char *filename, char dir[SECTOR_SIZE], char map[SECTOR_SIZE], FILE *floppy)
{
	int i, j, start, length, dirIndex;
	char input[8];

	dirIndex = getDirIndex(filename, dir);

	if (dirIndex < 0)
	{
		printf("ERROR: File not found, please make sure you typed the filename correctly\n");
		return;
	}

	printf("Are you sure you want to delete the file? (Y/n): ");
	fgets(input, 8, stdin);

	if (input[0] != 'Y' && input[0] != 'y')
	{
		return;
	}

	start = dir[dirIndex + 9];
	length = dir[dirIndex + 10];

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

	// write the map and directory back to the floppy image
	fseek(floppy, SECTOR_SIZE * 256, SEEK_SET);
	for (i = 0; i < SECTOR_SIZE; i++)
		fputc(map[i], floppy);

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