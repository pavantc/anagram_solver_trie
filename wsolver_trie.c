#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/time.h>

enum { 
        LVL_ERROR = 0,
        LVL_INFO,
        LVL_DEBUG,
        LVL_MAX
};

int cur_dbg_lvl = LVL_ERROR;

#define DEBUG_PRINT(lvl, msg...) \
do {\
	if (lvl <= cur_dbg_lvl) printf(msg); \
} while(0);

#define ASSERT(cond) {if (!(cond)) (*((volatile char *)0) = 0);}

#define MAX_RANK 26
#define WORD_DB "/usr/share/dict/words"
#define MAX_WORD_SIZE 80

struct list {
	char *word;
	struct list *next;
};
struct list *printed_wlist_head = NULL;

struct dict_node {
	/* Character represented by the node. -1 Represents root node */
	char c;
	/* Does this node end a complete word? */
	char cw;
	/* Pointers to all possible suffixes */
	struct dict_node *next[26];
};
typedef struct dict_node dict_node_t;

int __attribute__((always_inline))
get_rank(char c)
{
	return (tolower(c) - 'a');
}

/* Compare if two equally long strings are jumbles of each other */
int
compare_str(char *str1, char *str2)
{
	char t1[MAX_WORD_SIZE], t2[MAX_WORD_SIZE];
	int i, j, found;

	ASSERT(strlen(str1) == strlen(str2));
	strncpy(t1, str1, MAX_WORD_SIZE);
	strncpy(t2, str2, MAX_WORD_SIZE);

	for (i = 0; i < strlen(str1); i++) {
		found = 0;
		for (j = 0; j < strlen(str2); j++) {
			if (t2[j] == t1[i]) {
				found = 1;
				t2[j] = 0;
				break;
			}
		}
		if (!found) {
			return (1);
		}
	}

	return (0);
}

#if 0
/* print all words in the trie */
void
walk_dict(dict_node_t *dict, char *dict_word, int len)
{
	int i;

	if (!dict) {
		return;
	}

	if (dict->c == -1) {
		for (i = 0; i < MAX_RANK; i++) {
			if (dict->next[i]) {
				walk_dict(dict->next[i], dict_word, len);
			}
		}
		return;
	} else {
		*dict_word = dict->c;
	}

	if (dict->cw) {
		*(dict_word + 1) = '\0';
		printf("%s\n", dict_word - len);
	}

	for (i = 0; i < MAX_RANK; i++) {
		if (dict->next[i]) {
			DEBUG_PRINT(LVL_INFO, "dict node = %c, len = %d\n", dict->c, len);
			walk_dict(dict->next[i], dict_word + 1, len + 1);
		}
	}
}
#endif

void
walk_dict_and_find_match(dict_node_t *dict, char *dict_word, int dw_len, int
    kw_len, char *letter_list)
{
	int i;

	if (!dict) {
		return;
	}

	if (dict->c == -1) {
		for (i = 0; i < MAX_RANK; i++) {
			if (dict->next[i]) {
				walk_dict_and_find_match(dict->next[i],
				    dict_word, dw_len, kw_len, letter_list);
			}
		}
		return;
	} else {
		*dict_word = dict->c;
	}

	if (dw_len == kw_len - 1) {
		*(dict_word + 1) = '\0';
		if (dict->cw) {
			if (compare_str(dict_word - dw_len, letter_list) == 0) {
				printf("%s\n", dict_word - dw_len);
			}
		}
		return;
	}

	for (i = 0; i < MAX_RANK; i++) {
		if (dict->next[i]) {
			DEBUG_PRINT(LVL_INFO, "dict node = %c, dw_len = %d\n",
			    dict->c, dw_len);
			walk_dict_and_find_match(dict->next[i], dict_word + 1,
			    dw_len + 1, kw_len, letter_list);
		}
	}
}

int
search_dict(dict_node_t *dict, char *search_str)
{
	if (*search_str == '\0') {
		if (dict->cw) {
			return (0);
		} else {
			return (1);
		}
	} else {
		int r = get_rank(*search_str);
		DEBUG_PRINT(LVL_INFO, "search_str: %s, rank = %d, next = %p\n",
		    search_str, r, dict->next[r]);
		if (dict->next[r]) {
			return (search_dict(dict->next[r], search_str + 1));
		} else {
			return (1);
		}
	}
}

dict_node_t *
alloc_dict_node()
{
	dict_node_t *d = (dict_node_t *)malloc(sizeof (dict_node_t));
	if (d == NULL) {
		perror("malloc");
		exit(1);
	}
	return (d);
}

int
add_word_to_dict(dict_node_t *dict, char *word)
{
	int r, ret = 0;

	ASSERT(word != NULL);
	if (*word == '\0') {
		dict->cw = 1;
		DEBUG_PRINT(LVL_INFO, "Word completion\n");
		return (0);
	}

	r = get_rank(*word);
	DEBUG_PRINT(LVL_INFO, "Word : %s, rank = %d\n", word, r);
	if (dict->next[r] == NULL) {
		dict->next[r] = alloc_dict_node();
		if (dict->next[r] == NULL) {
			return (ENOMEM);
		}
		dict->next[r]->c = *word;
	}

	ret = add_word_to_dict(dict->next[r], word + 1);

	return (ret);
}

int
normalize(char *word)
{
	int i;

	for (i = 0; i < strlen(word); i++) {
		if (!isalpha(word[i])) {
			DEBUG_PRINT(LVL_DEBUG, "Non-standard: %s\n", word);
			return (1);
		} else {
			word[i] = tolower(word[i]);
		}
	}
	return (0);
}

int
construct_dict(dict_node_t *dict)
{
	/* Assumption : no word in the WORD_DB is >= MAX_WORD_SIZE characters long */
	FILE *fp;
	char word[MAX_WORD_SIZE];

	memset(dict, 0, sizeof (dict_node_t));
	dict->c = -1;

	fp = fopen(WORD_DB, "r");
	if (fp == NULL) {
		fprintf(stderr, "Could not open word database at : %s\n",
		    WORD_DB);
		exit(1);
	}

	while(fscanf(fp, "%s", word) != EOF) {
		DEBUG_PRINT(LVL_DEBUG, "Adding word: %s\n", word);
		if (normalize(word)) {
			continue;
		}
		if (add_word_to_dict(dict, word) != 0) {
			printf("Failed to add word '%s' to dict\n", word);
			exit(1);
		}
	}

	fclose(fp);
	return (0);
}

struct list *
get_wlist_node(char *str)
{
	struct list *temp = malloc(sizeof(struct list));
	if (temp) {
		temp->word = malloc(strlen(str));
		if (temp->word) {
			strcpy(temp->word, str);
			temp->next = NULL;
		} else {
			perror("malloc");
			exit(1);
		}
	} else {
		perror("malloc");
		exit(1);
	}
	return (temp);
}

int
search_printed_words(char *str)
{
	struct list *prev, *temp;

	if (printed_wlist_head == NULL) {
		printed_wlist_head = get_wlist_node(str);
		return (0);
	}

	for (prev = temp = printed_wlist_head; temp; prev = temp,
	    temp = temp->next) {
		if (strcmp(temp->word, str) == 0) {
			/* Found */
			return EEXIST;
		}
	}

	/* New word. Add to list */
	temp = get_wlist_node(str);
	prev->next = temp;
	return (0);
}

void
query_jumbled_word_from_user(char *jword)
{
	int i;
	printf("Enter jumbled word: ");
	fflush(stdin);
	fgets(jword, MAX_WORD_SIZE, stdin);
	/* fgets() reads the newline into the buffer. Remove if present */
	for (i = 0; i < strlen(jword); i++) {
		if (jword[i] == '\n') {
			jword[i] = '\0';
		}
	}
}

/*
 * There are different solutions to the word jumble problem:
 * One solution is to generate all permutations of the given string and verify
 * if each of there permutations is present in the dictionary or not. This is
 * not the best thing to do since the number of possible permutations grow to
 * very large numbers as the string size increases. For example, a string size
 * of 11 can lead to 11! permutations which is 39916800 (39 Million). An
 * increases of 1 in the string size leads to an order of magnitude increase in
 * the number of permutations. 12! = 479001600 (479 Million) and 13! is
 * 6227020800 (6.2 billion). If generation of each permutation involves a few
 * instructions (lets make an overtly optimistic estimate at 20 instructions
 * per permulation), that would amount to 120 billion instructions. Current day
 * CPUs might take (a theoretical value of) about 60 seconds to do this job. It
 * is evident that this methodology is impractical for string sizes greater
 * than 12.
 * 
 * A better bet is to walk through the dictionary and see which of the words in
 * the dictionary can be formed using the letters of the supplied string.
 * Once can construct a 'trie' to represent a dictionary, which can then be
 * used for efficient searches.
 */

void
solve_jumble(dict_node_t *dict, char *word)
{
	char temp[MAX_WORD_SIZE];
	int len = strlen(word);
	memset(temp, 0, sizeof (char));
	walk_dict_and_find_match(dict, temp, 0, len, word);
}

#define	TV_TO_MICRO(tv)	(tv.tv_sec * 1000000L + tv.tv_usec)
#define GET_TIME_DIFF(start_ts, end_ts)	(TV_TO_MICRO(end_ts) - TV_TO_MICRO(start_ts))

int
main()
{
	char word[MAX_WORD_SIZE];
	dict_node_t dictionary;
	struct timeval start_ts, end_ts;

#if 0
	/* TEST CODE */
	memset(&dictionary, 0, sizeof (dict_node_t));
	dictionary.c = -1;

	add_word_to_dict(&dictionary, "dictionary");
	add_word_to_dict(&dictionary, "doctor");
	add_word_to_dict(&dictionary, "mentor");
	add_word_to_dict(&dictionary, "tester");

	memset(word, 0, MAX_WORD_SIZE);
	//walk_dict(&dictionary, word, 0);
	//solve_jumble(&dictionary, "tetser");
	if (compare_str("abc", "bac") == 0) {
		printf("Jumbles\n");
	}
#else
	construct_dict(&dictionary);

	while(1) {
		query_jumbled_word_from_user(word);
		/*
		 * Normalize:
		 * Convert all upper case to lower case
		 * Exclude words with phoetic symbols (For ex. clichè)
		 */
		if (normalize(word)) {
			printf("Non-standard characters in %s\n", word);
			continue;
		}
		gettimeofday(&start_ts, NULL);
		solve_jumble(&dictionary, word);
		gettimeofday(&end_ts, NULL);
		printf("Time taken = %lu (microseconds)\n", GET_TIME_DIFF(start_ts, end_ts));
	}
#endif

	return (0);
}
