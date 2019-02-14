#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>

#include <fcntl.h>  
#include <io.h>

#define posix_memalign(p, a, s) (((*(p)) = _aligned_malloc((s), (a))), *(p) ?0 :errno)

#pragma comment(lib, "pthreadVC2.lib")

#define _FILE_OFFSET_BITS 64
#define MAX_STRING_LENGTH 1000

typedef double real;

typedef struct cooccur_rec {
	int word1;
	int word2;
	real val;
} CREC;

int write_header = 0; //0=no, 1=yes; writes vocab_size/vector_size as first line for use with some libraries, such as gensim.
int verbose = 2; // 0, 1, or 2
int use_unk_vec = 1; // 0 or 1
int num_threads = 8; // pthreads
int num_iter = 1; // Number of full passes through cooccurrence matrix
int vector_size = 100; // Word vector size
int save_gradsq = 0; // By default don't save squared gradient values
int use_binary = 0; // 0: save as text files; 1: save as binary; 2: both. For binary, save both word and context word vectors.
//int model = 2; // For text file output only. 0: concatenate word and context vectors (and biases) i.e. save everything; 1: Just save word vectors (no bias); 2: Save (word + context word) vectors (no biases)
int checkpoint_every = 0; // checkpoint the model for every checkpoint_every iterations. Do nothing if checkpoint_every <= 0
real eta = 0.05; // Initial learning rate
real alpha = 0.75, x_max = 100.0; // Weighting function parameters, not extremely sensitive to corpus, though may need adjustment for very small or very large corpora
real *W, *gradsq, *cost;
long long num_lines, *lines_per_thread, word_vocab_size,sense_vocab_size;
char *word_vocab_file,*sense_vocab_file, *input_file, *save_word_W_file,*save_sense_W_file, *save_word_gradsq_file, *save_sense_gradsq_file;

/* Efficient string comparison */
int scmp(char *s1, char *s2) {
	while (*s1 != '\0' && *s1 == *s2) { s1++; s2++; }
	return(*s1 - *s2);
}

void initialize_parameters() {
	long long a, b;
	vector_size++; // Temporarily increment to allocate space for bias

				   /* Allocate space for word vectors and context word vectors, and correspodning gradsq */
	a = posix_memalign((void **)&W, 128, (sense_vocab_size + word_vocab_size) * (vector_size + 1) * sizeof(real)); // Might perform better than malloc
	if (W == NULL) {
		fprintf(stderr, "Error allocating memory for W\n");
		exit(1);
	}
	a = posix_memalign((void **)&gradsq, 128, (sense_vocab_size + word_vocab_size) * (vector_size + 1) * sizeof(real)); // Might perform better than malloc
	if (gradsq == NULL) {
		fprintf(stderr, "Error allocating memory for gradsq\n");
		exit(1);
	}
	for (b = 0; b < vector_size; b++) for (a = 0; a < (sense_vocab_size + word_vocab_size); a++) W[a * vector_size + b] = (rand() / (real)RAND_MAX - 0.5) / vector_size;
	for (b = 0; b < vector_size; b++) for (a = 0; a < (sense_vocab_size + word_vocab_size); a++) gradsq[a * vector_size + b] = 1.0; // So initial value of eta is equal to initial learning rate
	vector_size--;
}

inline real check_nan(real update) {
	if (isnan(update) || isinf(update)) {
		fprintf(stderr, "\ncaught NaN in update");
		return 0.;
	}
	else {
		return update;
	}
}

/* Train the GloVe model */
void *glove_thread(void *vid) {
	long long a, b, l1, l2;
	long long id = *(long long*)vid;
	CREC cr;
	real diff, fdiff, temp1, temp2;
	FILE *fin;
	fin = fopen(input_file, "rb");
	_setmode(_fileno(fin), _O_BINARY);
	_fseeki64(fin, (num_lines / num_threads * id) * (sizeof(CREC)), SEEK_SET); //Threads spaced roughly equally throughout file
	cost[id] = 0;

	real* W_updates1 = (real*)malloc(vector_size * sizeof(real));
	real* W_updates2 = (real*)malloc(vector_size * sizeof(real));
	for (a = 0; a < lines_per_thread[id]; a++) {
		fread(&cr, sizeof(CREC), 1, fin);
		if (feof(fin)) break;
		if (cr.word1 < 1 || cr.word2 < 1) { continue; }

		/* Get location of words in W & gradsq */
		l1 = (cr.word1 - 1LL) * (vector_size + 1); // cr word indices start at 1
		l2 = ((cr.word2 - 1LL) + sense_vocab_size) * (vector_size + 1); // shift by vocab_size to get separate vectors for context words

																  /* Calculate cost, save diff for gradients */
		diff = 0;
		for (b = 0; b < vector_size; b++) diff += W[b + l1] * W[b + l2]; // dot product of word and context word vector
		diff += W[vector_size + l1] + W[vector_size + l2] - log(cr.val); // add separate bias for each word
		fdiff = (cr.val > x_max) ? diff : pow(cr.val / x_max, alpha) * diff; // multiply weighting function (f) with diff

																			 // Check for NaN and inf() in the diffs.
		if (isnan(diff) || isnan(fdiff) || isinf(diff) || isinf(fdiff)) {
			fprintf(stderr, "Caught NaN in diff for kdiff for thread. Skipping update");
			continue;
		}

		cost[id] += 0.5 * fdiff * diff; // weighted squared error

										/* Adaptive gradient updates */
		fdiff *= eta; // for ease in calculating gradient
		real W_updates1_sum = 0;
		real W_updates2_sum = 0;
		for (b = 0; b < vector_size; b++) {
			// learning rate times gradient for word vectors
			temp1 = fdiff * W[b + l2];
			temp2 = fdiff * W[b + l1];
			// adaptive updates
			W_updates1[b] = temp1 / sqrt(gradsq[b + l1]);
			W_updates2[b] = temp2 / sqrt(gradsq[b + l2]);
			W_updates1_sum += W_updates1[b];
			W_updates2_sum += W_updates2[b];
			gradsq[b + l1] += temp1 * temp1;
			gradsq[b + l2] += temp2 * temp2;
		}
		if (!isnan(W_updates1_sum) && !isinf(W_updates1_sum) && !isnan(W_updates2_sum) && !isinf(W_updates2_sum)) {
			for (b = 0; b < vector_size; b++) {
				W[b + l1] -= W_updates1[b];
				W[b + l2] -= W_updates2[b];
			}
		}

		// updates for bias terms
		W[vector_size + l1] -= check_nan(fdiff / sqrt(gradsq[vector_size + l1]));
		W[vector_size + l2] -= check_nan(fdiff / sqrt(gradsq[vector_size + l2]));
		fdiff *= fdiff;
		gradsq[vector_size + l1] += fdiff;
		gradsq[vector_size + l2] += fdiff;

	}
	free(W_updates1);
	free(W_updates2);

	fclose(fin);
	pthread_exit(NULL);
}

/* Save params to file */
int save_params(int nb_iter) {
	/*
	* nb_iter is the number of iteration (= a full pass through the cooccurrence matrix).
	*   nb_iter > 0 => checkpointing the intermediate parameters, so nb_iter is in the filename of output file.
	*   else        => saving the final paramters, so nb_iter is ignored.
	*/

	long long a, b;
	char format[20];
	char word_output_file[MAX_STRING_LENGTH], word_output_file_gsq[MAX_STRING_LENGTH], sense_output_file[MAX_STRING_LENGTH], sense_output_file_gsq[MAX_STRING_LENGTH];
	char *word = malloc(sizeof(char) * MAX_STRING_LENGTH + 1);
	FILE *fid_word, *fid_sense, *fout_word, *fout_sense;// *fgs_word, *fgs_sense;

/*	if (use_binary > 0) { // Save parameters in binary file
		if (nb_iter <= 0)
			sprintf(output_file, "%s.bin", save_W_file);
		else
			sprintf(output_file, "%s.%03d.bin", save_W_file, nb_iter);

		fout = fopen(output_file, "wb");
		_setmode(_fileno(fout), _O_BINARY);
		if (fout == NULL) { fprintf(stderr, "Unable to open file %s.\n", save_W_file); return 1; }
		for (a = 0; a < 2 * (long long)vocab_size * (vector_size + 1); a++) fwrite(&W[a], sizeof(real), 1, fout);
		fclose(fout);
		if (save_gradsq > 0) {
			if (nb_iter <= 0)
				sprintf(output_file_gsq, "%s.bin", save_gradsq_file);
			else
				sprintf(output_file_gsq, "%s.%03d.bin", save_gradsq_file, nb_iter);

			fgs = fopen(output_file_gsq, "wb");
			if (fgs == NULL) { fprintf(stderr, "Unable to open file %s.\n", save_gradsq_file); return 1; }
			_setmode(_fileno(fgs), _O_BINARY);
			for (a = 0; a < 2 * (long long)vocab_size * (vector_size + 1); a++) fwrite(&gradsq[a], sizeof(real), 1, fgs);
			fclose(fgs);
		}
	} */

	if (use_binary != 1) { // Save parameters in text file
		
		if (nb_iter <= 0) {
			sprintf(word_output_file, "%s.txt", save_word_W_file);
			sprintf(sense_output_file, "%s.txt", save_sense_W_file);
		}else{
			sprintf(sense_output_file, "%s.%03d.txt", save_sense_W_file, nb_iter);
			sprintf(word_output_file, "%s.%03d.txt", save_word_W_file, nb_iter);
		}
	/*	if (save_gradsq > 0) {
			if (nb_iter <= 0) {
				sprintf(word_output_file_gsq, "%s.txt", save_word_gradsq_file);
				sprintf(sense_output_file_gsq, "%s.txt", save_sense_gradsq_file);
			}
			else {
				sprintf(word_output_file_gsq, "%s.%03d.txt", save_word_gradsq_file, nb_iter);
				sprintf(sense_output_file_gsq, "%s.%03d.txt", save_sense_gradsq_file, nb_iter);
			}
				
			fgs_word = fopen(word_output_file_gsq, "wb");
			if (fgs_word == NULL) { fprintf(stderr, "Unable to open file %s.\n", save_word_gradsq_file); return 1; }

			fgs_sense = fopen(sense_output_file_gsq, "wb");
			if (fgs_sense == NULL) { fprintf(stderr, "Unable to open file %s.\n", save_sense_gradsq_file); return 1; }
		}
		*/

		fout_word = fopen(word_output_file, "wb");
		if (fout_word == NULL) { fprintf(stderr, "Unable to open file %s.\n", save_word_W_file); return 1; }
		fid_word = fopen(word_vocab_file, "r");
		sprintf(format, "%%%ds", MAX_STRING_LENGTH);
		if (fid_word == NULL) { fprintf(stderr, "Unable to open file %s.\n", word_vocab_file); return 1; }
		if (write_header) fprintf(fout_word, "%ld %d\n", word_vocab_size, vector_size);

		fout_sense = fopen(sense_output_file, "wb");
		if (fout_sense == NULL) { fprintf(stderr, "Unable to open file %s.\n", save_sense_W_file); return 1; }
		fid_sense = fopen(sense_vocab_file, "r");
	//	sprintf(format, "%%%ds", MAX_STRING_LENGTH);
		if (fid_sense == NULL) { fprintf(stderr, "Unable to open file %s.\n", sense_vocab_file); return 1; }
		if (write_header) fprintf(fout_sense, "%ld %d\n", sense_vocab_size, vector_size);
	
		for (a = 0; a < sense_vocab_size; a++) {
			if (fscanf(fid_sense, format, word) == 0) return 1;
			if (strcmp(word, "<unk>") == 0) return 1;
			fprintf(fout_sense, "%s", word);
			for (b = 0; b < vector_size; b++) fprintf(fout_sense, " %lf", W[a * (vector_size + 1) + b]);
			fprintf(fout_sense, "\n");
		
			/*	if (save_gradsq > 0) { // Save gradsq
				fprintf(fgs_sense, "%s", word);
				for (b = 0; b < (vector_size + 1); b++) fprintf(fgs_sense, " %lf", gradsq[a * (vector_size + 1) + b]);
				
				fprintf(fgs_sense, "\n");
			}*/

			if (fscanf(fid_sense, format, word) == 0) return 1; // Eat irrelevant frequency entry
		}

		for (a = 0; a < word_vocab_size; a++) {
			if (fscanf(fid_word, format, word) == 0) return 1;
			if (strcmp(word, "<unk>") == 0) return 1;
			fprintf(fout_word, "%s", word);
			for (b = 0; b < vector_size; b++) fprintf(fout_word, " %lf", W[(sense_vocab_size + a) * (vector_size + 1) + b]);
			fprintf(fout_word, "\n");
			if (fscanf(fid_word, format, word) == 0) return 1; // Eat irrelevant frequency entry
		}

		/*
		for (a = 0; a < sense_vocab_size; a++) {
			if (fscanf(fid, format, word) == 0) return 1;
			// input vocab cannot contain special <unk> keyword
			if (strcmp(word, "<unk>") == 0) return 1;
			fprintf(fout, "%s", word);
			if (model == 0) { // Save all parameters (including bias)
				for (b = 0; b < (vector_size + 1); b++) fprintf(fout, " %lf", W[a * (vector_size + 1) + b]);
				for (b = 0; b < (vector_size + 1); b++) fprintf(fout, " %lf", W[(vocab_size + a) * (vector_size + 1) + b]);
			}
			if (model == 1) // Save only "word" vectors (without bias)
				for (b = 0; b < vector_size; b++) fprintf(fout, " %lf", W[a * (vector_size + 1) + b]);
			if (model == 2) // Save "word + context word" vectors (without bias)
				for (b = 0; b < vector_size; b++) fprintf(fout, " %lf", W[a * (vector_size + 1) + b] + W[(vocab_size + a) * (vector_size + 1) + b]);
			fprintf(fout, "\n");
			if (save_gradsq > 0) { // Save gradsq
				fprintf(fgs, "%s", word);
				for (b = 0; b < (vector_size + 1); b++) fprintf(fgs, " %lf", gradsq[a * (vector_size + 1) + b]);
				for (b = 0; b < (vector_size + 1); b++) fprintf(fgs, " %lf", gradsq[(vocab_size + a) * (vector_size + 1) + b]);
				fprintf(fgs, "\n");
			}
			if (fscanf(fid, format, word) == 0) return 1; // Eat irrelevant frequency entry
		}

		if (use_unk_vec) {
			real* unk_vec = (real*)calloc((vector_size + 1), sizeof(real));
			real* unk_context = (real*)calloc((vector_size + 1), sizeof(real));
			word = "<unk>";

			int num_rare_words = vocab_size < 100 ? vocab_size : 100;

			for (a = vocab_size - num_rare_words; a < vocab_size; a++) {
				for (b = 0; b < (vector_size + 1); b++) {
					unk_vec[b] += W[a * (vector_size + 1) + b] / num_rare_words;
					unk_context[b] += W[(vocab_size + a) * (vector_size + 1) + b] / num_rare_words;
				}
			}

			fprintf(fout, "%s", word);
			if (model == 0) { // Save all parameters (including bias)
				for (b = 0; b < (vector_size + 1); b++) fprintf(fout, " %lf", unk_vec[b]);
				for (b = 0; b < (vector_size + 1); b++) fprintf(fout, " %lf", unk_context[b]);
			}
			if (model == 1) // Save only "word" vectors (without bias)
				for (b = 0; b < vector_size; b++) fprintf(fout, " %lf", unk_vec[b]);
			if (model == 2) // Save "word + context word" vectors (without bias)
				for (b = 0; b < vector_size; b++) fprintf(fout, " %lf", unk_vec[b] + unk_context[b]);
			fprintf(fout, "\n");

			free(unk_vec);
			free(unk_context);
		}
		*/

		fclose(fid_word);
		fclose(fid_sense);
		fclose(fout_word);
		fclose(fout_sense);
	//	if (save_gradsq > 0) fclose(fgs);
	}
	return 0;
}


/* Train model */
int train_glove() {
	long long a, file_size;
	int save_params_return_code;
	int b;
	FILE *fin;
	real total_cost = 0;

	fprintf(stderr, "TRAINING MODEL\n");

	fin = fopen(input_file, "rb");
	if (fin == NULL) { fprintf(stderr, "Unable to open cooccurrence file %s.\n", input_file); return 1; }
	_setmode(_fileno(fin), _O_BINARY);
	_fseeki64(fin, 0, SEEK_END);
	file_size = _ftelli64(fin);
	num_lines = file_size / (sizeof(CREC)); // Assuming the file isn't corrupt and consists only of CREC's
	fclose(fin);
	fprintf(stderr, "Read %lld lines.\n", num_lines);
	if (verbose > 1) fprintf(stderr, "Initializing parameters...");
	initialize_parameters();

	if (verbose > 1) fprintf(stderr, "done.\n");
	if (verbose > 0) fprintf(stderr, "vector size: %d\n", vector_size);
	if (verbose > 0) fprintf(stderr, "word_vocab size: %lld\n", word_vocab_size);
	if (verbose > 0) fprintf(stderr, "sense_vocab size: %lld\n", sense_vocab_size);
	if (verbose > 0) fprintf(stderr, "x_max: %lf\n", x_max);
	if (verbose > 0) fprintf(stderr, "alpha: %lf\n", alpha);
	pthread_t *pt = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
	lines_per_thread = (long long *)malloc(num_threads * sizeof(long long));

	time_t rawtime;
	struct tm *info;
	char time_buffer[80];
	// Lock-free asynchronous SGD
	for (b = 0; b < num_iter; b++) {
		total_cost = 0;
		for (a = 0; a < num_threads - 1; a++) lines_per_thread[a] = num_lines / num_threads;
		lines_per_thread[a] = num_lines / num_threads + num_lines % num_threads;
		long long *thread_ids = (long long*)malloc(sizeof(long long) * num_threads);
		for (a = 0; a < num_threads; a++) thread_ids[a] = a;
		for (a = 0; a < num_threads; a++) pthread_create(&pt[a], NULL, glove_thread, (void *)&thread_ids[a]);
		for (a = 0; a < num_threads; a++) pthread_join(pt[a], NULL);
		for (a = 0; a < num_threads; a++) total_cost += cost[a];
		free(thread_ids);

		time(&rawtime);
		info = localtime(&rawtime);
		strftime(time_buffer, 80, "%x - %I:%M.%S%p", info);
		fprintf(stderr, "%s, iter: %03d, cost: %lf\n", time_buffer, b + 1, total_cost / num_lines);

		if (checkpoint_every > 0 && (b + 1) % checkpoint_every == 0) {
			fprintf(stderr, "    saving itermediate parameters for iter %03d...", b + 1);
			save_params_return_code = save_params(b + 1);
			if (save_params_return_code != 0)
				return save_params_return_code;
			fprintf(stderr, "done.\n");
		}

	}
	free(pt);
	free(lines_per_thread);
	return save_params(0);
}

int main(int argc, char **argv) {
	int i;
	FILE *fid;
	word_vocab_file = malloc(sizeof(char) * MAX_STRING_LENGTH);
	sense_vocab_file = malloc(sizeof(char) * MAX_STRING_LENGTH);
	input_file = malloc(sizeof(char) * MAX_STRING_LENGTH);
	save_word_W_file = malloc(sizeof(char) * MAX_STRING_LENGTH);
	save_sense_W_file = malloc(sizeof(char) * MAX_STRING_LENGTH);
//	save_word_gradsq_file = malloc(sizeof(char) * MAX_STRING_LENGTH);
//	save_sense_gradsq_file = malloc(sizeof(char) * MAX_STRING_LENGTH);
	int result = 0;

	write_header = 0;
	verbose = 2;
	vector_size = 300;
	num_iter = 50;
	num_threads = 4;
	cost = malloc(sizeof(real) * num_threads);
	use_binary = 0;
//	model = 3;

	strcpy(word_vocab_file, (char *)"word_vocab.txt");
	strcpy(sense_vocab_file, (char *)"sense_vocab.txt");
	strcpy(save_word_W_file, (char *)"word_vectors");
	strcpy(save_sense_W_file, (char *)"sense_vectors");
	strcpy(input_file, (char *)"cooccurrence.shuf.bin");

	word_vocab_size = 0;
	fid = fopen(word_vocab_file, "r");
	if (fid == NULL) { fprintf(stderr, "Unable to open vocab file %s.\n", word_vocab_file); return 1; }
	while ((i = getc(fid)) != EOF) if (i == '\n') word_vocab_size++; // Count number of entries in vocab_file
	fclose(fid);

	sense_vocab_size = 0;
	fid = fopen(sense_vocab_file, "r");
	if (fid == NULL) { fprintf(stderr, "Unable to open vocab file %s.\n", sense_vocab_file); return 1; }
	while ((i = getc(fid)) != EOF) if (i == '\n') sense_vocab_size++; // Count number of entries in vocab_file
	fclose(fid);

	result = train_glove();
	free(cost);

	free(word_vocab_file);
	free(sense_vocab_file);
	free(input_file);
	free(save_word_W_file);
	free(save_sense_W_file);
//	free(save_gradsq_file);
	system("pause");
	return result;
}




