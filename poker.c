#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define Countof(x) ((sizeof (x)) / (sizeof *(x)))

enum suit {
	SPADES = 1,
	HEARTS,
	CLUBS,
	DIAMONDS,
};

enum {
	JACK = 11,
	QUEEN,
	KING,
	ACE,
};

const char *suit_names[] = { [SPADES] = "Spades", [HEARTS] = "Hearts", [CLUBS] = "Clubs", [DIAMONDS] = "Diamonds" };

struct card {
	enum suit suit;
	int value;
};

int cmp_card(const void *a_, const void *b_) {
	const struct card *a = a_, *b = b_;
	int t;
	t = a->value - b->value;
	if (t) return t;
	t = b->suit - a->suit;
	if (t) return t;
	return 0;
}

int cmp_card_by_suit(const void *a_, const void *b_) {
	const struct card *a = a_, *b = b_;
	int t;
	t = b->suit - a->suit;
	if (t) return t;
	t = a->value - b->value;
	if (t) return t;
	return 0;
}

void sort_hand(struct card *h, size_t n) {
	qsort(h, n, sizeof *h, cmp_card);
}

enum cat {
	INVALID = -1,
	STRAIGHT_FLUSH = 0,
	FOUR_KIND,
	FULL_HOUSE,
	FLUSH,
	STRAIGHT,
	THREE_KIND,
	TWO_PAIR,
	PAIR,
	HIGH_CARD,
};

const char *rank_names[] = {"straight flush", "four of a kind", "full house", "flush", "straight", "three of a kind", "two pair", "pair", "high card"};

struct rank {
	enum cat rank;
	struct card cards[5];
};

const struct rank invalid = { .rank = -1 };

int rank_is_valid(struct rank r) {
	return r.rank != INVALID;
}

int cmp_rank(const void *a_, const void *b_) {
	const struct rank *a = a_, *b = b_;
	int t;
	/* Invalid comes after everything */
	if (rank_is_valid(*a) != rank_is_valid(*b))
		return rank_is_valid(*a) ? -1 : 1;
	t = a->rank - b->rank;
	if (t) return t;
	for (unsigned i = 0; i < Countof(a->cards); i++) {
		t = b->cards[i].value - a->cards[i].value;
		if (t) return t;
	}
	return 0;
}
void p_card(struct card c);

struct rank check_flush(const struct card *h_, size_t n) {
	/* Everything else we might check finds it more convenient to sort by card
	 * value (officially rank, but the term is a little overloaded for me). For
	 * flushes, it's conceptually simpler (if not more efficient--I have no
	 * intention of checking) to sort by suit first. In order to not Fuck Shit
	 * Up, we must sort a copy. */
	struct card h[n];
	struct card last;
	unsigned i = 0;
	int run = 1;
	struct rank top = invalid;
	memcpy(h, h_, sizeof h);
	qsort(h, n, sizeof *h, cmp_card_by_suit);
	goto skip;
	do {
		struct card card = h[i];
		if (card.suit == last.suit) {
			run++;
			if (run >= 5) {
				struct rank rc = {FLUSH, {card, h[i - 1], h[i - 2], h[i - 3], h[i - 4]}};
				if (h[i - 4].value == card.value - 4 || (h[i - 4].value == 2 && h[i - 1].value == 5 && card.value == ACE)) {
					struct card high = card;
					if (h[i - 4].value == 2) {
						high = h[i - 1];
					}
					rc = (struct rank){STRAIGHT_FLUSH, {high}};
				}
				if (cmp_rank(&rc, &top) <= 0)
					top = rc;
			}
		} else {
			run = 1;
		}
skip:
		last = h[i];
	} while (++i < n);
	return top;
}

struct rank check_kind(const struct card *h, size_t n) {
	/* "N of a kind" */
	const enum cat run_cats[] = { [2] = PAIR, [3] = THREE_KIND, [4] = FOUR_KIND };
	struct card last;
	struct rank top = invalid;
	unsigned i = 0;
	int run = 1;
	int pos = -1;
	int top_run;
	goto skip;
	do {
		struct card card = h[i];
		if (card.value == last.value) {
			run++;
			if (run >= Countof(run_cats))
				abort();
			struct rank rc = {run_cats[run], {card}};
			if (cmp_rank(&rc, &top) < 0) {
				top = rc;
				pos = i;
				top_run = run;
			}
		} else {
			run = 1;
		}
skip:
		last = h[i];
	} while (++i < n);
	if (rank_is_valid(top)) {
		int cp = n - 1;
		for (int i = 1; i < Countof(top.cards); i++) {
			/* skip over the run we're using for the result */
			if (cp == pos)
				cp -= top_run;
			if (cp < 0)
				abort();
			top.cards[i] = h[cp];
			cp--;
		}
	}
	return top;
}

struct rank check_dual(const struct card *h, size_t n) {
	/* full house, two pair */
	/* we'll maintain two positions, major and minor, where major is the better
	 * one and the first to be filled */
	struct card last;
	int min = -1, min_run = 0;
	int maj = -1, maj_run = 0;
	unsigned i = 0;
	int run = 1;
	goto skip;
	do {
		struct card card = h[i];
		if (card.value == last.value) {
			run++;
			/* we need to figure out where to be */
			if (run >= 2) {
				int major = 0;
				if (maj == -1) major = 1;
				if (h[maj].value == card.value) major = 1;
				if (maj_run <= 2) major = 1;
				if (run > 2) major = 1;

				if (major) {
					if (maj != -1 && h[maj].value != card.value) {
						min = maj;
						min_run = maj_run;
					}
					maj = i;
					maj_run = run;
				} else {
					min = i;
					min_run = i;
				}
			}
		} else {
			run = 1;
		}
skip:
		last = h[i];
	} while (++i < n);
	if (maj_run >= 3 && min_run >= 2) {
		return (struct rank){FULL_HOUSE, {h[maj], h[min]}};
	} else if (maj_run >= 2 && min_run >= 2) {
		int cp = n - 1;
		struct rank r = (struct rank){TWO_PAIR, {h[maj], h[min]}};
		while (cp) {
			if (cp == maj) {
				cp -= 2;
				continue;
			}
			if (cp == min) {
				cp -= 2;
				continue;
			}
			break;
		}
		if (cp < 0)
			return invalid;
		r.cards[2] = h[cp];
		return r;
	}
	return invalid;
}

struct rank check_straight(const struct card *h, size_t n) {
	struct card last;
	struct card straight[5] = {h[0]};
	unsigned i = 0;
	int run = 1;
	struct rank top = invalid;
	goto skip;
	do {
		struct card card = h[i];
		if (card.value == last.value + 1 || (run == 4 && last.value == 5 && card.value == ACE)) {
			if (run <= 4) {
				straight[run] = card;
			} else {
				memmove(straight, straight + 1, sizeof (struct card) * 4);
				straight[4] = card;
			}
			run++;
			if (run >= 5) {
				struct rank rc = {STRAIGHT, {straight[4], straight[3], straight[2], straight[1], straight[0]}};
				if (cmp_rank(&rc, &top) < 0)
					top = rc;
			}
		} else if (card.value == last.value ) {
			straight[run - 1] = card;
		} else {
			run = 1;
			straight[0] = card;
		}
skip:
		last = h[i];
	} while (++i < n);
	return top;
}

struct {
	int best_rank;
	struct rank (*check)(const struct card *h, size_t n);
} checks[] = {
	{ STRAIGHT_FLUSH, check_flush },
	{ FOUR_KIND, check_kind },
	{ FULL_HOUSE, check_dual },
	{ STRAIGHT, check_straight },
};

struct rank rank_hand(const struct card *h, size_t n) {
	struct rank rank;
	for (unsigned i = 0; i < Countof(checks); i++) {
		if (!checks[i].check)
			continue;
		struct rank r = checks[i].check(h, n);
		if (i == 0 || cmp_rank(&r, &rank) < 0)
			rank = r;
		if (i + 1 < Countof(checks) && rank_is_valid(rank) && rank.rank < checks[i + 1].best_rank)
			break;
	}
	return rank;
}

void p_card(struct card c) {
	char num[20];
	char *val;
	switch (c.value) {
	case JACK: val = "Jack"; break;
	case QUEEN: val = "Queen"; break;
	case KING: val = "King"; break;
	case ACE: val = "Ace"; break;
	default:
		sprintf(num, "%d", c.value);
		val = num;
		break;
	}
	printf("%s of %s\n", val, suit_names[c.suit]);
}

void p_rank(struct rank r) {
	if (!rank_is_valid(r))
		puts("<none>");
	printf("%s: ", rank_names[r.rank]);
	for (unsigned i = 0; r.cards[i].suit && i < Countof(r.cards); i++) {
		p_card(r.cards[i]);
	}
}

int main(void) {
	struct rank r;
	struct card test_hand[] = {
//		{ SPADES, ACE },
		{ SPADES, QUEEN },
		{ HEARTS, JACK },
		{ CLUBS, JACK },
		{ SPADES, JACK },
//		{ DIAMONDS, JACK },
//		{ SPADES, 10 },
		{ SPADES, 9 },
		{ CLUBS, 9 },
		{ DIAMONDS, 9 },
		{ HEARTS, KING },
	};
	sort_hand(test_hand, Countof(test_hand));
	r = rank_hand(test_hand, Countof(test_hand));
	p_rank(r);
	return 0;
}
