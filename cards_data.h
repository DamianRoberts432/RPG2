#ifndef CARDS_DATA_H
#define CARDS_DATA_H
// Card sets. Each set lists its card names as one comma-separated string per
// flavor; InitCardSets() turns them into cards, and the Strategy Guide, the
// card shop and the card kid all read this table, so a set goes live in every
// one of them by uncommenting its row in card_set_table (cards_data.c).
enum { FL_PLAIN, FL_HOLY, FL_INFERNAL, FL_ARCANE, FL_MYTH };
typedef struct { const char *names; int flavor; } CardList;
typedef struct {
    const char *name, *blurb;
    int booster_price, box_price;
    CardList lists[3];
} CardSetDef;
extern const CardSetDef card_set_table[];
extern const int card_set_table_n;
extern const char *future_set_names[];
extern const int future_set_n;
#endif
