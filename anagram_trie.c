#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <sys/time.h>

enum { 
        LVL_ERROR = 0,
        LVL_INFO,
        LVL_DEBUG,
        LVL_MAX
};

int cur_dbg_lvl = LVL_INFO;

#define DEBUG_PRINT(lvl, msg...) \
do {\
	if (lvl <= cur_dbg_lvl) printf(msg); \
} while(0);

#define ASSERT(cond) {if (!(cond)) (*((volatile char *)0) = 0);}

#define MAX_RANK 26
#define WORD_DB "/usr/share/dict/words"
#define MAX_WORD_SIZE 80
#define MAX_ANAGRAM_SIZE 80
#define PLACE_HOLDER_CHAR '9'

struct list {
	char *word;
	struct list *next;
};

struct dict_node {
	/* Character represented by the node. -1 Represents root node */
	char c;
	/* Does this node end a complete word? */
	char cw;
	/* Pointers to all possible suffixes */
	struct dict_node *next[26];
};
typedef struct dict_node dict_node_t;

struct list *word_list_head = NULL;
int stack_top;
char *stack[MAX_ANAGRAM_SIZE];

int __attribute__((always_inline))
get_rank(char c)
{
	return (tolower(c) - 'a');
}

int
is_candidate_word(char *word, char *anagram)
{
	char copy[MAX_ANAGRAM_SIZE];
	int i, j, found;

	strncpy(copy, anagram, MAX_ANAGRAM_SIZE);

	for (i = 0; i < strlen(word); i++) {
		found = 0;
		for (j = 0; j < strlen(anagram); j++) {
			if (copy[j] == word[i]) {
				found = 1;
				copy[j] = 0;
				break;
			}
		}
		if (!found) {
			return (0);
		}
	}

	DEBUG_PRINT(LVL_DEBUG, "candidate_word, anagram: %s %s\n", word, anagram);
	return (1);
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
add_to_word_list(char *str)
{
	struct list *prev, *temp;

	if (word_list_head == NULL) {
		word_list_head = get_wlist_node(str);
		return (0);
	}

	for (prev = temp = word_list_head; temp;
	    prev = temp, temp = temp->next) {
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
walk_dict_and_add_to_word_list(dict_node_t *dict, char *dict_word, int len,
    char *anagram)
{
	int i;

	if (!dict) {
		return;
	}

	if (dict->c == -1) {
		for (i = 0; i < MAX_RANK; i++) {
			if (dict->next[i]) {
				walk_dict_and_add_to_word_list(dict->next[i],
				    dict_word, len, anagram);
			}
		}
		return;
	} else {
		*dict_word = dict->c;
	}

	if (dict->cw) {
		char *word = dict_word - len;
		*(dict_word + 1) = '\0';
		if (is_candidate_word(word, anagram)) {
			add_to_word_list(word);
		}
		DEBUG_PRINT(LVL_DEBUG, "dict_word: %s\n", word);
	}

	for (i = 0; i < MAX_RANK; i++) {
		if (dict->next[i]) {
			DEBUG_PRINT(LVL_DEBUG, "dict node = %c, len = %d\n", dict->c, len);
			walk_dict_and_add_to_word_list(dict->next[i],
			   dict_word + 1, len + 1, anagram);
		}
	}
}

void
generate_word_list(dict_node_t *dict, char *anagram)
{
	char buf[MAX_ANAGRAM_SIZE];
	memset(buf, 0, MAX_ANAGRAM_SIZE);
	walk_dict_and_add_to_word_list(dict, buf, 0, anagram);
}

dict_node_t *
alloc_dict_node()
{
	dict_node_t *d = (dict_node_t *)malloc(sizeof (dict_node_t));
	if (d == NULL) {
		perror("malloc");
		exit(1);
	}
	memset(d, 0, sizeof (dict_node_t));
	return (d);
}

int
add_word_to_dict(dict_node_t *dict, char *word)
{
	int r, ret = 0;

	ASSERT(word != NULL);
	if (*word == '\0') {
		dict->cw = 1;
		DEBUG_PRINT(LVL_DEBUG, "Word completion\n");
		return (0);
	}

	r = get_rank(*word);
	DEBUG_PRINT(LVL_DEBUG, "Word : %s, rank = %d\n", word, r);
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
normalize(char *anagram)
{
	int i;

	for (i = 0; i < strlen(anagram); i++) {
		if (!isalpha(anagram[i])) {
			DEBUG_PRINT(LVL_DEBUG, "Non-standard: %s\n", anagram);
			return (1);
		} else {
			anagram[i] = tolower(anagram[i]);
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

/* Check if string "str" is a subset of "clist" */
int
word_in_charlist(char *clist, char *str)
{
	char *p, *q;
	char copy_clist[MAX_WORD_SIZE];
	int found;

	strcpy(copy_clist, clist);

	for (p = str; *p != '\0'; p++) {
		found = 0;
		for (q = &copy_clist[0]; *q != '\0'; q++) {
			if (*p == *q) {
				*q = PLACE_HOLDER_CHAR;
				found = 1;
				break;
			}
		}
		if (!found) {
			return (0);
		}
	}
	strcpy(clist, copy_clist);
	return (1);
}

void
add_to_charlist(char *clist, char *str)
{
	char *p, *q;

	q = str;

	for (p = clist; *p != '\0'; p++) {
		if (*p == PLACE_HOLDER_CHAR) {
			if (*q == '\0') {
				/* We are done. Return. */
				return;
			} else {
				*p = *q;
				q++;
			}
		}
	}
}

void
init_stack()
{
	int i;
	stack_top = -1;
	for (i = 0; i < MAX_WORD_SIZE; i++) {
		stack[i] = malloc(MAX_WORD_SIZE);
		if (!stack[i]) {
			perror("malloc");
			exit(1);
		}
	}
}

void
push(char *str)
{
	strcpy(stack[++stack_top], str);
}

void
pop()
{
	stack_top--;
}

void
print_stack()
{
	int i;

	for (i = 0; i <= stack_top; i++) {
		printf("%s%s", stack[i], (i == stack_top ? "" : " "));
	}
	printf("\n");
}

void
get_anagrams(struct list *head, int len, char *charlist)
{
	struct list *temp;
	int wlen;

	if (head == NULL) {
		return;
	}

	for (temp = head; len && temp; temp = temp->next) {
		wlen = strlen(temp->word);
		if (wlen <= len && word_in_charlist(charlist, temp->word)) {
			push(temp->word);
			len -= wlen;
			if (len) {
				get_anagrams(temp->next, len, charlist);
			}
			if (len == 0) {
				print_stack();
			}
			pop();
			add_to_charlist(charlist, temp->word);
			len += wlen;
		}
	}
}

void cleanup_lists()
{
	struct list *cur, *next;

	for (cur = next = word_list_head; cur; ) {
		next = cur->next;
		free(cur->word);
		free(cur);
		cur = next;
	}
	word_list_head = NULL;
}

/* O(n^2) algorithm assuming that the wordlist is not too big */
void
sort_word_list(struct list *wlist)
{
	struct list *t1, *t2;
	char *temp;

	for (t1 = wlist; t1; t1 = t1->next) {
		for (t2 = t1; t2; t2 = t2->next) {
			if (strcmp(t1->word, t2->word) > 0) {
				temp = t1->word;
				t1->word = t2->word;
				t2->word = temp;
			}
		}
	}
}


	
void
usage(int argc, char **argv)
{
	fprintf(stderr, "usage: %s <string>\n", argv[0]);
	exit(1);
}

uint64_t
to_microsec(struct timeval *tv)
{
	return (tv->tv_sec * 1000000L + tv->tv_usec);
}

void
print_wordlist(struct list *head)
{
	struct list *temp;

	for (temp = head; temp; temp = temp->next) {
		printf("%s\n", temp->word);
	}
}

void
query_anagram_from_user(char *anagram)
{
	int i;
	printf("Enter anagram: ");
	fflush(stdin);
	fgets(anagram, MAX_WORD_SIZE, stdin);
	/* fgets() reads the newline into the buffer. Remove if present */
	for (i = 0; i < strlen(anagram); i++) {
		if (anagram[i] == '\n') {
			anagram[i] = '\0';
		}
	}
}

int
main()
{
	char anagram[MAX_ANAGRAM_SIZE];
	char copy[MAX_ANAGRAM_SIZE];
	dict_node_t dictionary;
	struct timeval c_start, c_end;
	struct timeval a_start, a_end;
	struct timeval s_start, s_end;
	uint64_t c_time, a_time, s_time;

	construct_dict(&dictionary);
	init_stack();

	while(1) {
		query_anagram_from_user(anagram);
		/*
		 * Normalize:
		 * Convert all upper case to lower case
		 * Exclude words with phoetic symbols (For ex. clichè)
		 */
		if (normalize(anagram)) {
			printf("Non-standard characters in %s\n", anagram);
			continue;
		}

		gettimeofday(&c_start, NULL);
		generate_word_list(&dictionary, anagram);
		gettimeofday(&c_end, NULL);

		gettimeofday(&s_start, NULL);
		sort_word_list(word_list_head);
		gettimeofday(&s_end, NULL);

		printf("\n\nPrinting sorted wordlist..\n");
		print_wordlist(word_list_head);

		printf("\n\nGenerating anagrams..\n");
		gettimeofday(&a_start, NULL);
		strncpy(copy, anagram, MAX_ANAGRAM_SIZE);
		get_anagrams(word_list_head, strlen(copy), copy);
		gettimeofday(&a_end, NULL);

		cleanup_lists();
		c_time = to_microsec(&c_end) - to_microsec(&c_start);
		s_time = to_microsec(&s_end) - to_microsec(&s_start);
		a_time = to_microsec(&a_end) - to_microsec(&a_start);
		printf("\n\n");
		fprintf(stderr, "time in microseconds for generating word list:%llu\n", c_time);
		fprintf(stderr, "time in microseconds for sort :%llu\n", s_time);
		fprintf(stderr, "time in microseconds for anagrams : %llu\n", a_time);
	}

	return (0);
}
