#include "cards_data.h"
#include "cards_future.h"

#define ANGELS_72 "Vehuiah,Jeliel,Sitael,Elemiah,Mahasiah,Lelahel,Achaiah,Cahetel,Haziel,Aladiah,Lauviah,Hahaiah," \
    "Iezalel,Mebahel,Hariel,Hekamiah,Leuviah,Caliel,Pahaliah,Nelchael,Ieiaiel,Melahel,Haheuiah,Nith-Haiah," \
    "Haaiah,Ierathel,Seheiah,Reiiel,Omael,Lecabel,Vasariah,Iehuiah,Lehahiah,Chavakiah,Menadel,Aniel," \
    "Haamiah,Rehael,Ieiazel,Hahahel,Mikael,Veuliah,Ielahiah,Sealiah,Ariel,Asaliah,Mihael,Vehuel," \
    "Daniel,Hahasiah,Imamiah,Nanael,Nithael,Mebahiah,Poiel,Nemamiah,Ieialel,Harahel,Mitzrael,Umabel," \
    "Iahhel,Anauel,Mehiel,Damabiah,Manakel,Eiael,Habuhiah,Rochel,Jabamiah,Haiaiel,Mumiah,Metatron"
#define GOETIA_72 "Bael,Agares,Vassago,Samigina,Marbas,Valefor,Amon,Barbatos,Paimon,Buer,Gusion,Sitri," \
    "Beleth,Leraje,Eligos,Zepar,Botis,Bathin,Sallos,Purson,Marax,Ipos,Aim,Naberius," \
    "Glasya-Labolas,Bune,Ronove,Berith,Astaroth,Forneus,Foras,Asmoday,Gaap,Furfur,Marchosias,Stolas," \
    "Phenex,Halphas,Malphas,Raum,Focalor,Vepar,Sabnock,Shax,Vine,Bifrons,Vual,Haagenti," \
    "Crocell,Furcas,Balam,Alloces,Caim,Murmur,Orobas,Gremory,Ose,Amy,Orias,Vapula," \
    "Zagan,Valac,Andras,Flauros,Andrealphus,Kimaris,Amdusias,Belial,Decarabia,Seere,Dantalion,Andromalius"
#define ARCANA_22 "The Fool,The Magician,The High Priestess,The Empress,The Emperor,The Hierophant,The Lovers," \
    "The Chariot,Strength,The Hermit,Wheel of Fortune,Justice,The Hanged Man,Death,Temperance,The Devil," \
    "The Tower,The Star,The Moon,The Sun,Judgement,The World"

// Set 0 (the Starter Set) is the bestiary monsters and wild animals defined
// in town.c; its row here only names it and prices its packs.
// To release a set: uncomment its row. Nothing else needs changing.
const CardSetDef card_set_table[] = {
    { "Starter Set", "The monsters and animals of the wild. Mostly plain pawns; only the big beasts have tricks.",
      2000, 30000, { { 0 } } },
    { "Angels & Goetia", "72 angels, the 72 Goetia demons and the 22 Major Arcana. Angels bless, demons curse.",
      20000, 350000, { { ANGELS_72, FL_HOLY }, { GOETIA_72, FL_INFERNAL }, { ARCANA_22, FL_ARCANE } } },
    // { "Greek Myths",    "Gods, heroes and monsters of Olympus.",          25000, 400000, { { GREEK_50, FL_MYTH } } },
    // { "Norse Myths",    "Asgard, the giants and the road to Ragnarok.",   25000, 400000, { { NORSE_50, FL_MYTH } } },
    // { "Egyptian Myths", "The gods of the Nile and the Duat.",              25000, 400000, { { EGYPT_50, FL_MYTH } } },
    // { "Celtic Myths",   "The Tuatha De Danann and the Otherworld.",        25000, 400000, { { CELTIC_50, FL_MYTH } } },
    // { "Japanese Myths", "Kami, yokai and the heroes of old Japan.",        25000, 400000, { { JAPAN_50, FL_MYTH } } },
    // { "Hindu Myths",    "Devas, asuras and the great epics.",              25000, 400000, { { HINDU_50, FL_MYTH } } },
    // { "Aztec Myths",    "The suns, the feathered serpent and Mictlan.",    25000, 400000, { { AZTEC_50, FL_MYTH } } },
    // { "Slavic Myths",   "Perun, Baba Yaga and the spirits of the forest.", 25000, 400000, { { SLAVIC_50, FL_MYTH } } },
    // { "Chinese Myths",  "The Jade Emperor's court and the Journey West.",  25000, 400000, { { CHINA_50, FL_MYTH } } },
};
const int card_set_table_n = (int)(sizeof(card_set_table) / sizeof(card_set_table[0]));

// Every planned set, in release order. The card kid hints at the first one
// that isn't in card_set_table yet.
const char *future_set_names[] = { "Greek Myths", "Norse Myths", "Egyptian Myths", "Celtic Myths", "Japanese Myths",
                                   "Hindu Myths", "Aztec Myths", "Slavic Myths", "Chinese Myths" };
const int future_set_n = (int)(sizeof(future_set_names) / sizeof(future_set_names[0]));
