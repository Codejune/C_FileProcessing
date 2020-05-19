#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "person.h"

#define RECORD_PER_PAGE (PAGE_SIZE / RECORD_SIZE)

/**
 * @brief Header structure
 */
struct Header
{
	int page_count;
	int record_count;
	int current_delete_page;
	int current_delete_record;
};

/**
 * @brief Delete information structure
 */
struct DeleteInfo
{
	int page;
	int record;
};

/**
 * @brief Read page from record file
 * @param *char $pagebuf Array of char to store the read buffer
 * @param int $pagenum Page number to read
 */
void readPage(FILE *fp, char *pagebuf, int pagenum)
{
	fseek(fp, PAGE_SIZE * pagenum, SEEK_SET);
	fread(pagebuf, PAGE_SIZE, 1, fp);
}

/**
 * @brief Save pagebuf in the pagenum location of the record file
 * @param fp Record file pointer
 * @param pagebuf Page buffer to store in record file
 * @param pagenum Page number to store in record file
 */
void writePage(FILE *fp, const char *pagebuf, int pagenum)
{
	fseek(fp, PAGE_SIZE * pagenum, SEEK_SET);
	fwrite(pagebuf, PAGE_SIZE, 1, fp);
}

/**
 * @brief Covert information read from structure to character array
 * @param recordbuf An array of characters to store information read from the sturcture
 * @param p Structure contatining information to read
 */
void pack(char *recordbuf, const Person *p)
{
	int remain;
	sprintf(recordbuf, "%s#%s#%s#%s#%s#%s#", 
			p->sn,
			p->name,
			p->age,
			p->addr,
			p->phone,
			p->email);
	remain = PAGE_SIZE - strlen(recordbuf) - 1;
	memset(recordbuf + strlen(recordbuf) + 1, (char)0xFF, remain);
}

/**
 * @brief Read information from recordbuf and convert it to a structure
 * @param recordbuf Buffer read from record file
 * @param p structure to store the read information
 */
void unpack(const char *recordbuf, Person *p)
{
	sscanf(recordbuf, "%[^#]%[^#]%[^#]%[^#]%[^#]%[^#]",
			p->sn,
			p->name,
			p->age,
			p->addr,
			p->phone,
			p->email);
}

/**
 * @brief Write a new record to the record file
 * @detail If the most recently deleted record exists, it is stored in that location, otherwise it is appended to the end of the record file.
 * @param fp Record file pointer
 * @param p structure to record data
 */
void insert(FILE *fp, const Person *p)
{
	char pagebuf[PAGE_SIZE];
	char recordbuf[RECORD_SIZE];
	struct Header header;
	struct DeleteInfo d_info;
	int record_idx;

	// Read header data
	fseek(fp, 0, SEEK_SET);
	fread(&header, sizeof(struct Header), 1, fp);

	pack(recordbuf, p); // Packing input data to record buffer

	if ((header.current_delete_page != -1) && (header.current_delete_record != -1)) { // Delete record is exist

		fseek(fp, header.current_delete_page * PAGE_SIZE + header.current_delete_record * RECORD_SIZE + sizeof(char), SEEK_SET);
		fread(&d_info, sizeof(struct DeleteInfo), 1, fp);

		readPage(fp, pagebuf, header.current_delete_page);
		memset(pagebuf + header.current_delete_record * RECORD_SIZE, (char)0xFF, RECORD_SIZE);
		strncpy(pagebuf + header.current_delete_record * RECORD_SIZE, recordbuf, strlen(recordbuf));
		writePage(fp, pagebuf, header.current_delete_page);

		header.current_delete_page = d_info.page;
		header.current_delete_record = d_info.record;

	} else { // Delete record doesn't exist

		// Get page count
		fseek(fp, 0, SEEK_END);
		record_idx = header.record_count % RECORD_PER_PAGE;

		if (record_idx == 0) { // Page fully

			printf("full!\n");

			// Create new page and write page to record file
			strncpy(pagebuf, recordbuf, strlen(recordbuf));
			memset(pagebuf + RECORD_SIZE, (char)0xFF, PAGE_SIZE - RECORD_SIZE);
			writePage(fp, pagebuf, header.page_count++);

		} else { // Page doesn't fully

			printf("doesn't full!\n");
			readPage(fp, pagebuf, header.page_count - 1);
			memset(pagebuf + record_idx * RECORD_SIZE, (char)0xFF, RECORD_SIZE);
			strncpy(pagebuf + record_idx * RECORD_SIZE, recordbuf, strlen(recordbuf));
			writePage(fp, pagebuf, header.page_count - 1);

		}
		header.record_count++;
	}


	// Refrest header data to record file
	memset(pagebuf, (char)0xFF, PAGE_SIZE);
	memcpy(pagebuf, &header, sizeof(struct Header)); // Convert header data to binary integer and write to page buffer
	writePage(fp, pagebuf, 0); // Write page buffer in record file
}

/**
 * @breif Find PERSON_ID data exist in record file and delete the record
 * @param fp Record file pointer
 * @param sn PERSON_ID
 */
void delete(FILE *fp, const char *sn)
{
	Person p;
	struct Header header;
	struct DeleteInfo d_info;
	char pagebuf[PAGE_SIZE];
	char recordbuf[RECORD_SIZE];
	int page_count;
	char symbol = '*';

	// Read header data
	fseek(fp, 0, SEEK_SET);
	fread(&header, sizeof(struct Header), 1, fp);

	// Get page count
	fseek(fp, 0, SEEK_END);
	page_count = ftell(fp) / PAGE_SIZE;

	for (int i = 1; i <= page_count; i++) { // Page index loop

		readPage(fp, pagebuf, i); // Read page and copy content to page buffer

		for (int j = 0; j < RECORD_PER_PAGE; j++) { // Record index loop

			strncpy(recordbuf, pagebuf + j * RECORD_SIZE, RECORD_SIZE); // Read page buffer and copy content to record buffer
			unpack(recordbuf, &p); // Convert record buffer content to structure

			if (strcmp(p.sn, sn) == 0) { // Find PERSON_ID

				fseek(fp, i * PAGE_SIZE + j * RECORD_SIZE, SEEK_SET);
				fwrite(&symbol, sizeof(char), 1, fp);
				fwrite(&header.current_delete_page, sizeof(int), 1, fp);
				fwrite(&header.current_delete_record, sizeof(int), 1, fp);

				// Refresh header and delete info structure
				header.record_count--;
				header.current_delete_page = i;
				header.current_delete_record = j;

				// Refrest header data to record file
				memset(pagebuf, (char)0xFF, PAGE_SIZE);
				memcpy(pagebuf, &header, sizeof(struct Header)); // Convert header data to binary integer and write to page buffer
				writePage(fp, pagebuf, 0); // Write page buffer in record file

				return;
			}
		}
	}
}

/**
 * @brief Record IO main function
 * @param argc Argument count
 * @param argv Argument string array
 */
int main(int argc, char *argv[])
{
	FILE *fp; // Record file pointer
	char pagebuf[PAGE_SIZE] = { 0 }; // Page buffer
	struct Header header; // Header data structure
	Person p; // Record data structure
	int remain;

	// Access error for record file
	if (access(argv[2], F_OK) < 0) {
		fp = fopen(argv[2], "w+"); // Record file creation and open
		// Initialize header structure
		header.page_count = 1;
		header.record_count = 0;
		header.current_delete_page = -1;
		header.current_delete_record = -1;
		// Make header page buffer
		memset(pagebuf, (char)0xFF, PAGE_SIZE);
		memcpy(pagebuf, &header, sizeof(struct Header)); // Convert header data to binary integer and write to page buffer
		writePage(fp, pagebuf, 0); // Write page buffer in record file
		fclose(fp); // Record file close
	}

	if (argc < 3) {
		fprintf(stderr, "invalid input\n");
		exit(1);
	}

	// Unknown option error
	if (strlen(argv[1]) > 2) {
		fprintf(stderr, "option error for %s\n", argv[1]);
		exit(1);
	}

	switch (argv[1][0]) {

		case 'i': // Insert option

			// Argument count error
			if (argc < 8) {
				fprintf(stderr, "Usage: %s i <FILE_NAME> <PERSION_ID> <NAME> <AGE> <ADDRESS> <PHONE_NUMBER> <EMAIL>\n", argv[0]);
				exit(1);
			}

			// Parse data and save to structure from the argument array
			if (strlen(argv[3]) > 16) {
				fprintf(stderr, "Out of bound PERSON_ID");
				break;
			}
			sscanf(argv[3], "%s", p.sn);

			if (strlen(argv[4]) > 20) {
				fprintf(stderr, "Out of bound NAME");
				break;
			}
			sscanf(argv[4], "%[^,\t\n]", p.name);

			if (strlen(argv[5]) > 6) {
				fprintf(stderr, "Out of bound AGE");
				break;
			}
			sscanf(argv[5], "%s", p.age);

			if (strlen(argv[6]) > 24) {
				fprintf(stderr, "Out of bound ADDRESS");
				break;
			}
			sscanf(argv[6], "%s", p.addr);

			if (strlen(argv[7]) > 18) {
				fprintf(stderr, "Out of bound PHONE_NUMBER");
				break;
			}
			sscanf(argv[7], "%s", p.phone);

			if (strlen(argv[8]) > 28) {
				fprintf(stderr, "Out of bound EMAIL");
				break;
			}
			sscanf(argv[8], "%s", p.email);

			// Open record file
			if ((fp = fopen(argv[2], "r+")) < 0) {
				fprintf(stderr, "fopen error for %s\n", argv[2]);
				exit(1);
			}

			// Insert to record file
			insert(fp, &p);
			fclose(fp);
			break;

		case 'd': // Delete option

			// Open record file
			if ((fp = fopen(argv[2], "r+")) < 0) {
				fprintf(stderr, "fopen error for %s\n", argv[2]);
				exit(1);
			}
			sscanf(argv[3], "%s", p.sn); // Parse data and save to structure from the argument array

			// Delete record from record file
			delete(fp, p.sn);	
			break;

		default: // Unknown option

			fprintf(stderr, "Unknown option\n");
			break;
	}
	exit(0);
}
