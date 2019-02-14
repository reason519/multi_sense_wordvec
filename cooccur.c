#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <fcntl.h>  
#include <io.h>

#define TSIZE 1048576
#define SEED 1159241
#define HASHFN bitwisehash

static const int MAX_STRING_LENGTH = 1000;
#define _MAX_STRING_LENGTH 1000
typedef double real;

typedef struct cooccur_rec {
	int word1;
	int word2;
	real val;
} CREC;

typedef struct cooccur_rec_id {
	int word1;
	int word2;
	real val;
	int id;
} CRECID;

typedef struct hashrec {
	char	*word;
	long long id;
	struct hashrec *next;
} HASHREC;

typedef struct word_sense_id {
	int w1;  //target word id
	int w2;  //context word id 
} WSID;

int verbose = 2; // 0, 1, or 2
long long max_product; // Cutoff for product of word frequency ranks below which cooccurrence counts will be stored in a compressed full array
long long overflow_length; // Number of cooccurrence records whose product exceeds max_product to store in memory before writing to disk
int window_size = 15; // default context window size
real memory_limit = 2; // soft limit, in gigabytes, used to estimate optimal array sizes
char *word_vocab_file,*sense_vocab_file, *file_head,*wiki_file,*cooccurrence_file;

/* Efficient string comparison */
int scmp(char *s1, char *s2) {
	while (*s1 != '\0' && *s1 == *s2) { s1++; s2++; }
	return(*s1 - *s2);
}

/* Move-to-front hashing and hash function from Hugh Williams, http://www.seg.rmit.edu.au/code/zwh-ipl/ */

/* Simple bitwise hash function */
unsigned int bitwisehash(char *word, int tsize, unsigned int seed) {
	char c;
	unsigned int h;
	h = seed;
	//fprintf(stderr, "ss%s", word);
	for (; (c = *word) != '\0'; word++) h ^= ((h << 5) + c + (h >> 2));
	return((unsigned int)((h & 0x7fffffff) % tsize));
}

/* Create hash table, initialise pointers to NULL */
HASHREC ** inithashtable() {
	int	i;
	HASHREC **ht;
	ht = (HASHREC **)malloc(sizeof(HASHREC *) * TSIZE);
	for (i = 0; i < TSIZE; i++) ht[i] = (HASHREC *)NULL;
	return(ht);
}

/* Search hash table for given string, return record if found, else NULL */
HASHREC *hashsearch(HASHREC **ht, char *w) {
	HASHREC	*htmp, *hprv;
	unsigned int hval = HASHFN(w, TSIZE, SEED);
	for (hprv = NULL, htmp = ht[hval]; htmp != NULL && scmp(htmp->word, w) != 0; hprv = htmp, htmp = htmp->next);
	if (htmp != NULL && hprv != NULL) { // move to front on access
		hprv->next = htmp->next;
		htmp->next = ht[hval];
		ht[hval] = htmp;
	}
	return(htmp);
}

/* Insert string in hash table, check for duplicates which should be absent */
void hashinsert(HASHREC **ht, char *w, long long id) {
	HASHREC	*htmp, *hprv;
	unsigned int hval = HASHFN(w, TSIZE, SEED);
	for (hprv = NULL, htmp = ht[hval]; htmp != NULL && scmp(htmp->word, w) != 0; hprv = htmp, htmp = htmp->next);
	if (htmp == NULL) {
		htmp = (HASHREC *)malloc(sizeof(HASHREC));
		htmp->word = (char *)malloc(strlen(w) + 1);
		strcpy(htmp->word, w);
		htmp->id = id;
		htmp->next = NULL;
		if (hprv == NULL) ht[hval] = htmp;
		else hprv->next = htmp;
	}
	else fprintf(stderr, "Error, duplicate entry located: %s.\n", htmp->word);
	return;
}

/* Read word from input stream */
int get_word(char *word, FILE *fin) {
	int i = 0, ch;
	while (!feof(fin)) {
		ch = fgetc(fin);
		if (ch == 13) continue;
		if ((ch == ' ') || (ch == '\t') || (ch == '\n')) {
			if (i > 0) {
				if (ch == '\n') ungetc(ch, fin);
				break;
			}
			if (ch == '\n') return 1;
			else continue;
		}
		word[i++] = ch;
		if (i >= MAX_STRING_LENGTH - 1) i--;   // truncate words that exceed max length
	}
	word[i] = 0;
	return 0;
}

/* Write sorted chunk of cooccurrence records to file, accumulating duplicate entries */
int write_chunk(CREC *cr, long long length, FILE *fout) {
	if (length == 0) return 0;

	long long a = 0;
	CREC old = cr[a];

	for (a = 1; a < length; a++) {
		if (cr[a].word1 == old.word1 && cr[a].word2 == old.word2) {
			old.val += cr[a].val;
			continue;
		}
		fwrite(&old, sizeof(CREC), 1, fout);
		old = cr[a];
	}
	fwrite(&old, sizeof(CREC), 1, fout);
	return 0;
}

/* Check if two cooccurrence records are for the same two words, used for qsort */
int compare_crec(const void *a, const void *b) {
	int c;
	if ((c = ((CREC *)a)->word1 - ((CREC *)b)->word1) != 0) return c;
	else return (((CREC *)a)->word2 - ((CREC *)b)->word2);

}

/* Check if two cooccurrence records are for the same two words */
int compare_crecid(CRECID a, CRECID b) {
	int c;
	if ((c = a.word1 - b.word1) != 0) return c;
	else return a.word2 - b.word2;
}

/* Swap two entries of priority queue */
void swap_entry(CRECID *pq, int i, int j) {
	CRECID temp = pq[i];
	pq[i] = pq[j];
	pq[j] = temp;
}

/* Insert entry into priority queue */
void insert(CRECID *pq, CRECID new, int size) {
	int j = size - 1, p;
	pq[j] = new;
	while ((p = (j - 1) / 2) >= 0) {
		if (compare_crecid(pq[p], pq[j]) > 0) { swap_entry(pq, p, j); j = p; }
		else break;
	}
}

/* Delete entry from priority queue */
void delete(CRECID *pq, int size) {
	int j, p = 0;
	pq[p] = pq[size - 1];
	while ((j = 2 * p + 1) < size - 1) {
		if (j == size - 2) {
			if (compare_crecid(pq[p], pq[j]) > 0) swap_entry(pq, p, j);
			return;
		}
		else {
			if (compare_crecid(pq[j], pq[j + 1]) < 0) {
				if (compare_crecid(pq[p], pq[j]) > 0) { swap_entry(pq, p, j); p = j; }
				else return;
			}
			else {
				if (compare_crecid(pq[p], pq[j + 1]) > 0) { swap_entry(pq, p, j + 1); p = j + 1; }
				else return;
			}
		}
	}
}

int find_split(char*s) {
	while (*s != '\0') {
		if (*s == '|') {
			return 1;
		}
		else {
			s++;
		}
	}
	return 0;
}

/* Write top node of priority queue to file, accumulating duplicate entries */
int merge_write(CRECID new, CRECID *old, FILE *fout) {
	if (new.word1 == old->word1 && new.word2 == old->word2) {
		old->val += new.val;
		return 0; // Indicates duplicate entry
	}
	fwrite(old, sizeof(CREC), 1, fout);
	*old = new;
	return 1; // Actually wrote to file
}

/* Merge [num] sorted files of cooccurrence records */
int merge_files(int num) {
	int i, size;
	long long counter = 0;
	CRECID *pq, new, old;
	char filename[200];
	FILE **fid, *fout;
	fid = malloc(sizeof(FILE) * num);
	pq = malloc(sizeof(CRECID) * num);
//	fout = stdout;
	if (verbose > 1) fprintf(stderr, "Reading cooccur file \"%s\"...\n", cooccurrence_file);
	fout = fopen(cooccurrence_file, "wb");
	if (fout == NULL) { fprintf(stderr, "Unable to open wiki file %s.\n", cooccurrence_file); return 1; }

	_setmode(_fileno(fout), _O_BINARY);
	if (verbose > 1) fprintf(stderr, "Merging cooccurrence files: processed 0 lines.");

	/* Open all files and add first entry of each to priority queue */
	for (i = 0; i < num; i++) {
		sprintf(filename, "%s_%04d.bin", file_head, i);
		fid[i] = fopen(filename, "rb");
		if (fid[i] == NULL) { fprintf(stderr, "Unable to open file %s.\n", filename); return 1; }
		_setmode(_fileno(fid[i]), _O_BINARY);
		fread(&new, sizeof(CREC), 1, fid[i]);
		new.id = i;
		insert(pq, new, i + 1);
	}

	/* Pop top node, save it in old to see if the next entry is a duplicate */
	size = num;
	old = pq[0];
	i = pq[0].id;
	delete(pq, size);
	fread(&new, sizeof(CREC), 1, fid[i]);
	if (feof(fid[i])) size--;
	else {
		new.id = i;
		insert(pq, new, size);
	}

	/* Repeatedly pop top node and fill priority queue until files have reached EOF */
	while (size > 0) {
		counter += merge_write(pq[0], &old, fout); // Only count the lines written to file, not duplicates
		if ((counter % 100000) == 0) if (verbose > 1) fprintf(stderr, "\033[39G%lld lines.", counter);
		i = pq[0].id;
		delete(pq, size);
		fread(&new, sizeof(CREC), 1, fid[i]);
		if (feof(fid[i])) size--;
		else {
			new.id = i;
			insert(pq, new, size);
		}
	}
	fwrite(&old, sizeof(CREC), 1, fout);
	fprintf(stderr, "\033[0GMerging cooccurrence files: processed %lld lines.\n", ++counter);
	for (i = 0; i<num; i++) {
		sprintf(filename, "%s_%04d.bin", file_head, i);
		remove(filename);
	}
	fprintf(stderr, "\n");
	fclose(fout);
	return 0;
}

/* Collect word-word cooccurrence counts from input stream */
int get_cooccurrence() {
	int flag, x, y, fidcounter = 1;
	long long a, j = 0, k, id, counter = 0, ind = 0, word_vocab_size,sense_vocab_size, w1, w2, *lookup;
	char format[20], filename[200], str[_MAX_STRING_LENGTH + 1];
	char *word_str, *sense_str;
	FILE *fid, *foverflow;
	real *bigram_table, r;
	HASHREC *word_htmp,*sense_htmp, **word_vocab_hash = inithashtable(),**sense_vocab_hash=inithashtable();
	CREC *cr = malloc(sizeof(CREC) * (overflow_length + 1));
	WSID *history;
	history = malloc(sizeof(WSID) * window_size);

	fprintf(stderr, "COUNTING COOCCURRENCES\n");
	if (verbose > 0) {
		fprintf(stderr, "window size: %d\n", window_size);
	//	if (symmetric == 0) fprintf(stderr, "context: asymmetric\n");
	//	else fprintf(stderr, "context: symmetric\n");
	}
	if (verbose > 1) fprintf(stderr, "max product: %lld\n", max_product);
	if (verbose > 1) fprintf(stderr, "overflow length: %lld\n", overflow_length);
	sprintf(format, "%%%ds %%lld", MAX_STRING_LENGTH); // Format to read from vocab file, which has (irrelevant) frequency data
//	fprintf(stderr, "%s\n", format); 
	if (verbose > 1) fprintf(stderr, "Reading word vocab from file \"%s\"...", word_vocab_file);
	fid = fopen(word_vocab_file, "r");
	if (fid == NULL) { fprintf(stderr, "Unable to open word vocab file %s.\n", word_vocab_file); return 1; }
	while (fscanf(fid, format, str, &id) != EOF) hashinsert(word_vocab_hash, str, ++j); // Here id is not used: inserting vocab words into hash table with their frequency rank, j
	fclose(fid);
	word_vocab_size = j;
	j = 0;

	if (verbose > 1) fprintf(stderr, "Reading sense vocab from file \"%s\"...", sense_vocab_file);
	fid = fopen(sense_vocab_file, "r");
	if (fid == NULL) { fprintf(stderr, "Unable to open sense vocab file %s.\n", sense_vocab_file); return 1; }
	while (fscanf(fid, format, str, &id) != EOF) hashinsert(sense_vocab_hash, str, ++j); // Here id is not used: inserting vocab words into hash table with their frequency rank, j
	fclose(fid);
	sense_vocab_size = j;
	j = 0;

	if (verbose > 1) fprintf(stderr, "loaded %lld words.\nBuilding lookup table...", sense_vocab_size);

	/* Build auxiliary lookup table used to index into bigram_table */
	lookup = (long long *)calloc(sense_vocab_size + 1, sizeof(long long));
	if (lookup == NULL) {
		fprintf(stderr, "Couldn't allocate memory!");
		return 1;
	}
	lookup[0] = 1;
	for (a = 1; a <= sense_vocab_size; a++) {
		if ((lookup[a] = max_product / a) < sense_vocab_size) lookup[a] += lookup[a - 1];
		else lookup[a] = lookup[a - 1] + sense_vocab_size;
	}
	if (verbose > 1) fprintf(stderr, "table contains %lld elements.\n", lookup[a - 1]);

	/* Allocate memory for full array which will store all cooccurrence counts for words whose product of frequency ranks is less than max_product */
	bigram_table = (real *)calloc(lookup[a - 1], sizeof(real));
	if (bigram_table == NULL) {
		fprintf(stderr, "Couldn't allocate memory!");
		return 1;
	}

//	fid = stdin;
	if (verbose > 1) fprintf(stderr, "Reading wiki file \"%s\"...\n", wiki_file);
	fid = fopen(wiki_file, "r");
	if (fid == NULL) { fprintf(stderr, "Unable to open wiki file %s.\n", wiki_file); return 1; }
	

	sprintf(format, "%%%ds", MAX_STRING_LENGTH);
	sprintf(filename, "%s_%04d.bin", file_head, fidcounter);
	foverflow = fopen(filename, "w");
	_setmode(_fileno(foverflow), _O_BINARY);
	if (verbose > 1) fprintf(stderr, "Processing token: 0");

	/* For each token in input stream, calculate a weighted cooccurrence sum within window_size */
	while (1) {
		
		if (ind >= overflow_length - window_size) { // If overflow buffer is (almost) full, sort it and write it to temporary file
			qsort(cr, ind, sizeof(CREC), compare_crec);
			write_chunk(cr, ind, foverflow);
			fclose(foverflow);
			fidcounter++;
			sprintf(filename, "%s_%04d.bin", file_head, fidcounter);
			foverflow = fopen(filename, "w");
			_setmode(_fileno(foverflow), _O_BINARY);
			ind = 0;
			
		}

		flag = get_word(str, fid);
		//	fprintf(stderr, "sss%d", flag);
		if (feof(fid)) break;
		if (flag == 1) { j = 0; continue; } // Newline, reset line index (j)
		counter++;
		if ((counter % 1000000) == 0) if (verbose > 1) fprintf(stderr, "\033[19G%lld", counter);

	//	fprintf(stderr, "%s", str);
		//对str进行处理，看是否包含|
		int flag_line = find_split(str);
		char *temp = strtok(str, "|");
		if (flag_line) {
			word_str = temp;
			temp = strtok(NULL, "|");
			sense_str = temp;
		}
		else {
			word_str = temp;
			sense_str = temp;
		}
		if (word_str == NULL || sense_str == NULL) continue;
		word_htmp = hashsearch(word_vocab_hash, word_str);
		sense_htmp = hashsearch(sense_vocab_hash, sense_str);
		if (word_htmp == NULL||sense_htmp==NULL) continue; // Skip out-of-vocabulary words



		w1 = sense_htmp->id; // Target word (frequency rank)
		w2 = word_htmp->id;  //Context word
		for (k = j - 1; k >= ((j > window_size) ? j - window_size : 0); k--) { // Iterate over all words to the left of target word, but not past beginning of line
		//	w1 = history[k % window_size].w1; // Context word (frequency rank)
			if (history[k % window_size].w2 < max_product / w1) { // Product is small enough to store in a full array
				bigram_table[lookup[w1 - 1] + history[k % window_size].w2 - 2] += 1.0 / ((real)(j - k)); // Weight by inverse of distance between words
			}else { // Product is too big, data is likely to be sparse. Store these entries in a temporary buffer to be sorted, merged (accumulated), and written to file when it gets full.
				cr[ind].word1 = w1;
				cr[ind].word2 = history[k % window_size].w2;
				cr[ind].val = 1.0 / ((real)(j - k));
				ind++; // Keep track of how full temporary buffer is
			}
			if (w2 < max_product / history[k % window_size].w1) { // Product is small enough to store in a full array
				bigram_table[lookup[history[k % window_size].w1 - 1] + w2 - 2] += 1.0 / ((real)(j - k)); // Weight by inverse of distance between words
			}
			else { // Product is too big, data is likely to be sparse. Store these entries in a temporary buffer to be sorted, merged (accumulated), and written to file when it gets full.
				cr[ind].word1 = history[k % window_size].w1;
				cr[ind].word2 = w2;
				cr[ind].val = 1.0 / ((real)(j - k));
				ind++; // Keep track of how full temporary buffer is
			}
		}
		history[j % window_size].w1 = w1; // Target word is stored in circular buffer to become context word in the future
		history[j % window_size].w2 = w2;
		j++;
	}

	/* Write out temp buffer for the final time (it may not be full) */
	if (verbose > 1) fprintf(stderr, "\033[0GProcessed %lld tokens.\n", counter);
	qsort(cr, ind, sizeof(CREC), compare_crec);
	write_chunk(cr, ind, foverflow);
	sprintf(filename, "%s_0000.bin", file_head);

	/* Write out full bigram_table, skipping zeros */
	if (verbose > 1) fprintf(stderr, "Writing cooccurrences to disk");
	fid = fopen(filename, "w");
	_setmode(_fileno(fid), _O_BINARY);
	j = 1e6;
	for (x = 1; x <= sense_vocab_size; x++) {
		if ((long long)(0.75*log(sense_vocab_size / x)) < j) { j = (long long)(0.75*log(sense_vocab_size / x)); if (verbose > 1) fprintf(stderr, "."); } // log's to make it look (sort of) pretty
		for (y = 1; y <= (lookup[x] - lookup[x - 1]); y++) {
			if ((r = bigram_table[lookup[x - 1] - 2 + y]) != 0) {
				fwrite(&x, sizeof(int), 1, fid);
				fwrite(&y, sizeof(int), 1, fid);
				fwrite(&r, sizeof(real), 1, fid);
			}
		}
	}


	if (verbose > 1) fprintf(stderr, "%d files in total.\n", fidcounter + 1);
	fclose(fid);
	fclose(foverflow);
	free(cr);
	free(lookup);
	free(bigram_table);
	free(word_vocab_hash);
	free(sense_vocab_hash);

	return merge_files(fidcounter + 1); // Merge the sorted temporary files
}

int main(int argc, char **argv) {
	int i;
	real rlimit, n = 1e5;
	word_vocab_file = malloc(sizeof(char) * MAX_STRING_LENGTH);
	sense_vocab_file = malloc(sizeof(char) * MAX_STRING_LENGTH);
	file_head = malloc(sizeof(char) * MAX_STRING_LENGTH);
	wiki_file= malloc(sizeof(char) * MAX_STRING_LENGTH);
	cooccurrence_file = malloc(sizeof(char) * MAX_STRING_LENGTH);

	verbose = 2;
	window_size = 15;

	strcpy(word_vocab_file, (char *)"word_vocab.txt");
	strcpy(sense_vocab_file, (char *)"sense_vocab.txt");
	strcpy(wiki_file, (char *)"F:\\毕业实验\\wiki_all.txt");
	strcpy(cooccurrence_file, (char *)"cooccurrence.bin");

	strcpy(file_head, (char *)"overflow");
	memory_limit = 4;  //2g

	/* The memory_limit determines a limit on the number of elements in bigram_table and the overflow buffer */
	/* Estimate the maximum value that max_product can take so that this limit is still satisfied */
	rlimit = 0.85 * (real)memory_limit * 1073741824 / (sizeof(CREC));
	while (fabs(rlimit - n * (log(n) + 0.1544313298)) > 1e-3) n = rlimit / (log(n) + 0.1544313298);
	max_product = (long long)n;
	overflow_length = (long long)rlimit / 6; // 0.85 + 1/6 ~= 1


	get_cooccurrence();
	system("pause");
	return 0;
}