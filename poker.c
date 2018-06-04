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
	STRAIGHT_FLUSH,
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
	int rank;
	struct card cards[5];
};

const struct rank invalid = { .rank = -1 };

int cmp_rank(const void *a_, const void *b_) {
	const struct rank *a = a_, *b = b_;
	int t;
	/* Invalid comes after everything */
	if ((a->rank == -1) != (b->rank == -1))
		return a->rank == -1 ? 1 : -1;
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
	/* "N of a kind", perhaps also pairs */
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
				memmove(straight, straight + 1, sizeof *straight * 4);
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
	{ FOUR_KIND, 0 /*check_kind*/ },
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
		if (i + 1 < Countof(checks) && rank.rank != -1 && rank.rank < checks[i + 1].best_rank)
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
	if (r.rank == -1) {
		puts("<none>");
	}
	printf("%s: ", rank_names[r.rank]);
	for (unsigned i = 0; r.cards[i].suit && i < Countof(r.cards); i++) {
		p_card(r.cards[i]);
	}
}

int main(void) {
	struct rank r;
	struct card test_hand[] = {
		{ SPADES, ACE },
		{ SPADES, QUEEN },
		{ HEARTS, JACK },
		{ CLUBS, JACK },
		{ SPADES, 10 },
		{ SPADES, 9 },
		{ HEARTS, KING },
	};
	sort_hand(test_hand, Countof(test_hand));
	for (int i = 0; i < Countof(test_hand); i++) {
		p_card(test_hand[i]);
	}
	r = rank_hand(test_hand, Countof(test_hand));
	p_rank(r);
	return 0;
}
