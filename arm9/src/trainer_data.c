#include "global.h"
#include "heap.h"
#include "trainer_data.h"
#include "math_util.h"
#include "party.h"
#include "pokemon.h"
#include "proto.h"
#include "msgdata.h"
#include "constants/trainer_classes.h"
#include "unk_02024E64.h"
#include "use_item_on_mon.h"

#pragma thumb on

void MonEncryptSegment(u16 * datap, u32 size, u32 key);
void MonDecryptSegment(u16 * datap, u32 size, u32 key);
PokemonDataBlock * GetSubstruct(struct BoxPokemon * boxmon, u32 personality, u8 which_struct);
u16 MonEncryptionLCRNG(u32 * seed);

#define ENCRY_ARGS_PTY(mon) (u16 *)&(mon)->party, sizeof((mon)->party), (mon)->box.pid
#define ENCRY_ARGS_BOX(boxmon) (u16 *)&(boxmon)->substructs, sizeof((boxmon)->substructs), (boxmon)->checksum
#define ENCRYPT_PTY(mon) MonEncryptSegment(ENCRY_ARGS_PTY(mon))
#define ENCRYPT_BOX(boxmon) MonEncryptSegment(ENCRY_ARGS_BOX(boxmon))
#define DECRYPT_PTY(mon) MonDecryptSegment(ENCRY_ARGS_PTY(mon))
#define DECRYPT_BOX(boxmon) MonDecryptSegment(ENCRY_ARGS_BOX(boxmon))

#define SUBSTRUCT_CASE(v1, v2, v3, v4)                                  \
{                                                                       \
        PokemonDataBlock *substructs = boxMon->substructs;              \
        switch (substructType)                                          \
        {                                                               \
        case 0:                                                         \
            result = &substructs[v1];                                   \
            break;                                                      \
        case 1:                                                         \
            result = &substructs[v2];                                   \
            break;                                                      \
        case 2:                                                         \
            result = &substructs[v3];                                   \
            break;                                                      \
        case 3:                                                         \
            result = &substructs[v4];                                   \
            break;                                                      \
        }                                                               \
                                                                        \
        break;                                                          \
}

u16 CalcMonChecksum(u16 * data, u32 size);
#define CHECKSUM(boxmon) CalcMonChecksum((u16 *)(boxmon)->substructs, sizeof((boxmon)->substructs))

struct PlayerParty partyold;
extern int trainerbattle;
int randomize = 1; // if 0, does not randomize your team

struct PlayerParty* GetOldParty() {
    return &partyold;
}

void rndlvlPoke(struct Pokemon * mon, int lvl) {
    u32 exp;
    int species;
    u32 growthrate;
    u32 personality;
    u16 nickname[11];
    u16 helditem;
    // BOOL decry;
    lvl = lvl+5;

    species = GetMonData(mon, MON_DATA_SPECIES, NULL);
    if (species == SPECIES_EGG) return;
    if (GetMonData(mon, MON_DATA_LEVEL, NULL) != lvl) {
        growthrate = (u32)GetMonBaseStat(species, BASE_GROWTH_RATE);
        exp = GetMonExpBySpeciesAndLevel(species, lvl);
        SetMonData(mon, MON_DATA_EXPERIENCE, &exp);
        SetMonData(mon, MON_DATA_LEVEL, &lvl);
        CalcMonStats(mon);
    }
    if (randomize==1){
        u16 n = LCRandom() % (NUM_SPECIES - 1) + 1;

        // save relevant mon data
        personality = GetMonData(mon, MON_DATA_PERSONALITY, NULL);
        GetMonData(mon, MON_DATA_NICKNAME, nickname);
        helditem = GetMonData(mon, MON_DATA_HELD_ITEM, NULL);
        lvl = GetMonData(mon, MON_DATA_LEVEL, NULL);
        // exp = GetMonExpBySpeciesAndLevel((int)n, lvl);

        // create new pokemon and write data
        CreateMon(mon, n, lvl, 32, 1, &personality, OT_ID_PLAYER_ID, 0);
        // SetMonData(mon, MON_DATA_EXPERIENCE, &exp);

        DECRYPT_BOX(&mon->box);
        mon->box.checksum = CHECKSUM(&mon->box);
        ENCRYPT_BOX(&mon->box);

        SetMonData(mon, MON_DATA_NICKNAME, nickname);
        SetMonData(mon, MON_DATA_HELD_ITEM, &helditem);
    }
}

void RandomizeAndLevel(struct PlayerParty * party, int lvl) {
    int nummons = GetPartyCount(party);
    int i;
    for (i=0; i<nummons; i++) {
        rndlvlPoke(&party->mons[i], lvl);
    }
}

void fixOTdata(struct Pokemon * mon, u16 otid, u16 otTrainerName[8], u8 trainergender) {
    BOOL decry;
    u16 checksum;

    decry = AcquireMonLock(mon);
    SetMonData(mon, MON_DATA_OTID, &otid);
    checksum = CHECKSUM(&mon->box);
    mon->box.checksum = checksum;
    ReleaseMonLock(mon, decry);
    SetMonData(mon, MON_DATA_OT_NAME, otTrainerName);
    SetMonData(mon, MON_DATA_MET_GENDER, &trainergender);
}

void switchparties2(struct PlayerParty * party, struct PlayerParty * enemyparty, int doublebattle, struct PlayerParty * party3)
{
    // swap teams (single battles only)
    int playerMons = GetPartyCount(party);
    int enemyMons = GetPartyCount(enemyparty);
    int p3moncount;
    int i;
    u16 otid;
    // u16 dexNum;
    struct Pokemon tempMon;
    // u32 value;
    u16 otTrainerName[8];
    u8 trainergender;

    if (doublebattle>0) {
        p3moncount = GetPartyCount(party3);
    }
    if (enemyMons==0) return; // wild battle with partner

    // why won't this work???
    otid = GetMonData(&party->mons[0], MON_DATA_OTID, NULL);
    GetMonData(&party->mons[0], MON_DATA_OT_NAME, otTrainerName);
    trainergender = (u8)GetMonData(&party->mons[0], MON_DATA_MET_GENDER, NULL);

    // TODO deal with double battles
    struct Pokemon *mon;
    struct Pokemon *mon2;
    for (i=0; i<6; i++) {
        mon = &party->mons[i];
        if (doublebattle==0) {
            mon2 = &enemyparty->mons[i];
        }
        else {
            if (enemyMons+p3moncount<=6){
                if ((i<enemyMons) && (i<(playerMons-1))) {
                    mon2 = &enemyparty->mons[i];
                }
                else if (i>=(playerMons-1)) {
                    if ((enemyMons>=playerMons) && (i>=(p3moncount+playerMons-1))){
                        mon2 = &enemyparty->mons[playerMons-1+(i-p3moncount-1)];
                    }
                    else mon2 = &party3->mons[i-(playerMons-1)];
                }
                else {
                    if (enemyMons<playerMons) mon2 = &party3->mons[i-enemyMons];
                    else mon2 = &party3->mons[i-(enemyMons-1)];
                }
            }
            else mon2 = &enemyparty->mons[i];
        }
        // only swap if there's a mon2
        if (doublebattle==0) {
            if (i>=enemyMons) {
                tempMon = *mon2;
                // *mon2 = *mon;
                *mon = tempMon;
            }
            else {
                tempMon = *mon2;
                *mon2 = *mon;
                *mon = tempMon;
                if (i<enemyMons) fixOTdata(mon, otid, otTrainerName, trainergender);
            }
        } else {
            if (enemyMons+p3moncount>=playerMons) {
                tempMon = *mon2;
                *mon2 = *mon;
                *mon = tempMon;
                if (i<enemyMons+p3moncount) fixOTdata(mon, otid, otTrainerName, trainergender);
            }
            // else if (enemyMons+p3moncount<playerMons) {
            else{
                tempMon = *mon2;
                // *mon2 = *mon;
                *mon = tempMon;
                if (i<enemyMons+p3moncount) fixOTdata(mon, otid, otTrainerName, trainergender);
            }
        }
    }
    if (doublebattle==0) {
        party->curCount = enemyMons;
    }
    else {
        if (enemyMons+p3moncount>6) party->curCount=6;
        else party->curCount=(enemyMons+p3moncount);
    }
    enemyparty->curCount = playerMons;
}

int isDoubles(struct BattleSetupStruct * enemies) {
    // returns 
    // 1 if normal double battle (vs 2 separate trainers)
    // 2 if multi battle (i have a teammate)
    int slot3mons = GetPartyCount(enemies->parties[2]);
    int slot4mons = GetPartyCount(enemies->parties[3]);
    if (slot3mons>0) slot3mons=1;
    if (slot4mons>0) slot4mons=1;
    return slot3mons+slot4mons;
}

// Loads all battle opponents, including multi-battle partner if exists.
void EnemyTrainerSet_Init(struct BattleSetupStruct * enemies, struct SaveBlock2 * sav2, u32 heap_id)
{
    struct TrainerDataLoaded trdata;
    struct MsgData * msgData;
    u16 * rivalName;
    s32 i;
    struct String * str;
    u8 opponentmaxlvl;
    // u16 otid;

    msgData = NewMsgDataFromNarc(1, NARC_MSGDATA_MSG, 559, heap_id);
    rivalName = GetRivalNamePtr(FUN_02024EC0(sav2));
    for (i = 0; i < 4; i++)
    {
        if (enemies->trainer_idxs[i] != 0)
        {
            TrainerData_ReadTrData(enemies->trainer_idxs[i], &trdata.data);
            enemies->datas[i] = trdata;
            if (trdata.data.trainerClass == TRAINER_CLASS_PKMN_TRAINER_BARRY)
            {
                CopyU16StringArray(enemies->datas[i].name, rivalName);
            }
            else
            {
                str = NewString_ReadMsgData(msgData, enemies->trainer_idxs[i]);
                CopyStringToU16Array(str, enemies->datas[i].name, OT_NAME_LENGTH + 1);
                String_dtor(str);
            }
            CreateNPCTrainerParty(enemies, i, heap_id);
        }
    }
    enemies->flags |= trdata.data.doubleBattle;
    trainerbattle=1;
    int doublebattle = isDoubles(enemies);
    HealParty(enemies->parties[0]);
    opponentmaxlvl = Party_GetMaxLevel(enemies->parties[1]);
    // opponentmaxlvl = 20;
    RandomizeAndLevel(enemies->parties[0], (int)opponentmaxlvl);
    partyold = *enemies->parties[0];
    switchparties2(enemies->parties[0], enemies->parties[1], doublebattle, enemies->parties[3]);
    DestroyMsgData(msgData);
}

s32 TrainerData_GetAttr(u32 tr_idx, u32 attr_no)
{
    struct TrainerDataLoaded trainer;
    s32 ret;

    TrainerData_ReadTrData(tr_idx, &trainer.data);
    switch (attr_no)
    {
    case 0:
        ret = trainer.data.trainerType;
        break;
    case 1:
        ret = trainer.data.trainerClass;
        break;
    case 2:
        ret = trainer.data.unk_2;
        break;
    case 3:
        ret = trainer.data.npoke;
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        attr_no -= 4;
        ret = trainer.data.items[attr_no];
        break;
    case 8:
        ret = (s32)trainer.data.unk_C;
        break;
    case 9:
        ret = (s32)trainer.data.doubleBattle;
        break;
    }
    return ret; // UB: uninitialized in event of invalid attr
}

// Relevant files:
//   files/poketool/trmsg/trtbl.narc
//   files/poketool/trmsg/trtblofs.narc
//   files/msgdata/msg/narc_0558.txt
// trtbl is a single-member NARC whose entries are two shorts each. The first short
// designates the trainer ID and the second the message ID. They are ordered the same
// as the corresponding msgdata file. All messages for a given trainer are found together,
// however the trainers are not in order in this file. trtblofs gives a pointer into trtbl
// for each trainer. trtblofs is also a single-member NARC whose entries are shorts, one
// per NPC trainer.
BOOL TrainerMessageWithIdPairExists(u32 trainer_idx, u32 msg_id, u32 heap_id)
{
    u16 rdbuf[3];
    struct NARC * trTblNarc;
    BOOL ret = FALSE;
    u32 trTblSize;

    trTblSize = GetNarcMemberSizeByIdPair(NARC_POKETOOL_TRMSG_TRTBL, 0);
    ReadFromNarcMemberByIdPair(&rdbuf[0], NARC_POKETOOL_TRMSG_TRTBLOFS, 0, trainer_idx * 2, 2);
    trTblNarc = NARC_ctor(NARC_POKETOOL_TRMSG_TRTBL, heap_id);
    while (rdbuf[0] != trTblSize)
    {
        NARC_ReadFromMember(trTblNarc, 0, rdbuf[0], 4, &rdbuf[1]);
        if (rdbuf[1] == trainer_idx && rdbuf[2] == msg_id)
        {
            ret = TRUE;
            break;
        }
        if (rdbuf[1] != trainer_idx)
            break;
        rdbuf[0] += 4;
    }
    NARC_dtor(trTblNarc);
    return ret;
}

void GetTrainerMessageByIdPair(u32 trainer_idx, u32 msg_id, struct String * str, u32 heap_id)
{
    u16 rdbuf[3];
    u32 trTblSize;
    struct NARC * trTblNarc;

    trTblSize = GetNarcMemberSizeByIdPair(NARC_POKETOOL_TRMSG_TRTBL, 0);
    ReadFromNarcMemberByIdPair(&rdbuf[0], NARC_POKETOOL_TRMSG_TRTBLOFS, 0, trainer_idx * 2, 2);
    trTblNarc = NARC_ctor(NARC_POKETOOL_TRMSG_TRTBL, heap_id);
    while (rdbuf[0] != trTblSize)
    {
        NARC_ReadFromMember(trTblNarc, 0, rdbuf[0], 4, &rdbuf[1]);
        if (rdbuf[1] == trainer_idx && rdbuf[2] == msg_id)
        {
            ReadMsgData_NewNarc_ExistingString(NARC_MSGDATA_MSG, 558, (u32)(rdbuf[0] / 4), heap_id, str);
            break;
        }
        rdbuf[0] += 4;
    }
    NARC_dtor(trTblNarc);
    if (rdbuf[0] == trTblSize)
        StringSetEmpty(str);
}

void TrainerData_ReadTrData(u32 idx, struct TrainerData * dest)
{
    ReadWholeNarcMemberByIdPair(dest, NARC_POKETOOL_TRAINER_TRDATA, (s32)idx);
}

void TrainerData_ReadTrPoke(u32 idx, union TrainerMon * dest)
{
    ReadWholeNarcMemberByIdPair(dest, NARC_POKETOOL_TRAINER_TRPOKE, (s32)idx);
}

const u8 sTrainerClassGenderCountTbl[] = {
    /*TRAINER_CLASS_PKMN_TRAINER_M*/             0,
    /*TRAINER_CLASS_PKMN_TRAINER_F*/             1,
    /*TRAINER_CLASS_YOUNGSTER*/                  0,
    /*TRAINER_CLASS_LASS*/                       1,
    /*TRAINER_CLASS_CAMPER*/                     0,
    /*TRAINER_CLASS_PICNICKER*/                  1,
    /*TRAINER_CLASS_BUG_CATCHER*/                0,
    /*TRAINER_CLASS_AROMA_LADY*/                 1,
    /*TRAINER_CLASS_TWINS*/                      1,
    /*TRAINER_CLASS_HIKER*/                      0,
    /*TRAINER_CLASS_BATTLE_GIRL*/                1,
    /*TRAINER_CLASS_FISHERMAN*/                  0,
    /*TRAINER_CLASS_CYCLIST_M*/                  0,
    /*TRAINER_CLASS_CYCLIST_F*/                  1,
    /*TRAINER_CLASS_BLACK_BELT*/                 0,
    /*TRAINER_CLASS_ARTIST*/                     0,
    /*TRAINER_CLASS_PKMN_BREEDER_M*/             0,
    /*TRAINER_CLASS_PKMN_BREEDER_F*/             1,
    /*TRAINER_CLASS_COWGIRL*/                    1,
    /*TRAINER_CLASS_JOGGER*/                     0,
    /*TRAINER_CLASS_POKEFAN_M*/                  0,
    /*TRAINER_CLASS_POKEFAN_F*/                  1,
    /*TRAINER_CLASS_POKE_KID*/                   1,
    /*TRAINER_CLASS_YOUNG_COUPLE*/               2,
    /*TRAINER_CLASS_ACE_TRAINER_M*/              0,
    /*TRAINER_CLASS_ACE_TRAINER_F*/              1,
    /*TRAINER_CLASS_WAITRESS*/                   1,
    /*TRAINER_CLASS_VETERAN*/                    0,
    /*TRAINER_CLASS_NINJA_BOY*/                  0,
    /*TRAINER_CLASS_DRAGON_TAMER*/               0,
    /*TRAINER_CLASS_BIRD_KEEPER*/                1,
    /*TRAINER_CLASS_DOUBLE_TEAM*/                2,
    /*TRAINER_CLASS_RICH_BOY*/                   0,
    /*TRAINER_CLASS_LADY*/                       1,
    /*TRAINER_CLASS_GENTLEMAN*/                  0,
    /*TRAINER_CLASS_SOCIALITE*/                  1,
    /*TRAINER_CLASS_BEAUTY*/                     1,
    /*TRAINER_CLASS_COLLECTOR*/                  0,
    /*TRAINER_CLASS_POLICEMAN*/                  0,
    /*TRAINER_CLASS_PKMN_RANGER_M*/              0,
    /*TRAINER_CLASS_PKMN_RANGER_F*/              1,
    /*TRAINER_CLASS_SCIENTIST*/                  0,
    /*TRAINER_CLASS_SWIMMER_M*/                  0,
    /*TRAINER_CLASS_SWIMMER_F*/                  1,
    /*TRAINER_CLASS_TUBER_M*/                    0,
    /*TRAINER_CLASS_TUBER_F*/                    1,
    /*TRAINER_CLASS_SAILOR*/                     0,
    /*TRAINER_CLASS_SIS_AND_BRO*/                2,
    /*TRAINER_CLASS_RUIN_MANIAC*/                0,
    /*TRAINER_CLASS_PSYCHIC_M*/                  0,
    /*TRAINER_CLASS_PSYCHIC_F*/                  1,
    /*TRAINER_CLASS_PI*/                         0,
    /*TRAINER_CLASS_GUITARIST*/                  0,
    /*TRAINER_CLASS_ACE_TRAINER_SNOW_M*/         0,
    /*TRAINER_CLASS_ACE_TRAINER_SNOW_F*/         1,
    /*TRAINER_CLASS_SKIER_M*/                    0,
    /*TRAINER_CLASS_SKIER_F*/                    1,
    /*TRAINER_CLASS_ROUGHNECK*/                  0,
    /*TRAINER_CLASS_CLOWN*/                      0,
    /*TRAINER_CLASS_WORKER*/                     0,
    /*TRAINER_CLASS_SCHOOL_KID_M*/               0,
    /*TRAINER_CLASS_SCHOOL_KID_F*/               1,
    /*TRAINER_CLASS_LEADER_ROARK*/               0,
    /*TRAINER_CLASS_PKMN_TRAINER_BARRY*/         0,
    /*TRAINER_CLASS_LEADER_BYRON*/               0,
    /*TRAINER_CLASS_ELITE_FOUR_AARON*/           0,
    /*TRAINER_CLASS_ELITE_FOUR_BERTHA*/          1,
    /*TRAINER_CLASS_ELITE_FOUR_FLINT*/           0,
    /*TRAINER_CLASS_ELITE_FOUR_LUCIEN*/          0,
    /*TRAINER_CLASS_CHAMPION*/                   1,
    /*TRAINER_CLASS_BELLE__PA*/                  2,
    /*TRAINER_CLASS_RANCHER*/                    0,
    /*TRAINER_CLASS_COMMANDER_MARS*/             1,
    /*TRAINER_CLASS_GALACTIC*/                   0,
    /*TRAINER_CLASS_LEADER_GARDENIA*/            1,
    /*TRAINER_CLASS_LEADER_WAKE*/                0,
    /*TRAINER_CLASS_LEADER_MAYLENE*/             1,
    /*TRAINER_CLASS_LEADER_FANTINA*/             1,
    /*TRAINER_CLASS_LEADER_CANDICE*/             1,
    /*TRAINER_CLASS_LEADER_VOLKNER*/             0,
    /*TRAINER_CLASS_PARASOL_LADY*/               1,
    /*TRAINER_CLASS_WAITER*/                     0,
    /*TRAINER_CLASS_INTERVIEWERS*/               2,
    /*TRAINER_CLASS_CAMERAMAN*/                  0,
    /*TRAINER_CLASS_REPORTER*/                   1,
    /*TRAINER_CLASS_IDOL*/                       1,
    /*TRAINER_CLASS_GALACTIC_BOSS*/              0,
    /*TRAINER_CLASS_COMMANDER_JUPITER*/          1,
    /*TRAINER_CLASS_COMMANDER_SATURN*/           1,
    /*TRAINER_CLASS_GALACTIC_F*/                 1,
    /*TRAINER_CLASS_PKMN_TRAINER_CHERYL*/        1,
    /*TRAINER_CLASS_PKMN_TRAINER_RILEY*/         0,
    /*TRAINER_CLASS_PKMN_TRAINER_MARLEY*/        1,
    /*TRAINER_CLASS_PKMN_TRAINER_BUCK*/          0,
    /*TRAINER_CLASS_PKMN_TRAINER_MIRA*/          1,
    /*TRAINER_CLASS_PKMN_TRAINER_LUCAS*/         0,
    /*TRAINER_CLASS_PKMN_TRAINER_DAWN*/          1,
    /*TRAINER_CLASS_TOWER_TYCOON*/               0
};

// Returns 0 for male, 1 for female, 2 for doubles. See above vector.
int TrainerClass_GetGenderOrTrainerCount(int a0)
{
    return sTrainerClassGenderCountTbl[a0];
}

void CreateNPCTrainerParty(struct BattleSetupStruct * enemies, s32 party_id, u32 heap_id)
{
    union TrainerMon * data;
    s32 i;
    s32 j;
    u32 seed_bak;
    struct Pokemon * pokemon;
    struct TrainerMonSpeciesItemMoves * monSpeciesItemMoves;
    struct TrainerMonSpeciesItem * monSpeciesItem;
    struct TrainerMonSpeciesMoves * monSpeciesMoves;
    struct TrainerMonSpecies * monSpecies;
    u32 seed;
    u32 personality;
    u8 iv;
    u32 pid_gender;

    // We abuse the RNG for personality value generation, so back up the overworld
    // state
    seed_bak = GetLCRNGSeed();
    InitPartyWithMaxSize(enemies->parties[party_id], PARTY_SIZE);
    data = (union TrainerMon *)AllocFromHeap(heap_id, sizeof(union TrainerMon) * PARTY_SIZE);
    pokemon = AllocMonZeroed(heap_id);
    TrainerData_ReadTrPoke(enemies->trainer_idxs[party_id], data);

    // If a Pokemon's gender ratio is 50/50, the generated Pokemon will be the same
    // gender as its trainer. Otherwise, it will assume the more abundant gender
    // according to its species gender ratio. In double battles, the behavior is
    // identical to that of a solitary male opponent.
    pid_gender = (u32)((TrainerClass_GetGenderOrTrainerCount(enemies->datas[party_id].data.trainerClass) == 1) ? 0x78 : 0x88);

    // The trainer types can be more efficiently and expandibly treated as a flag
    // array, with bit 0 being custom moveset and bit 1 being held item.
    // Nintendo didn't do it that way, instead using a switch statement and a lot
    // of code duplication. This has been the case since the 2nd generation games.
    switch (enemies->datas[party_id].data.trainerType)
    {
    case TRTYPE_MON:
    {
        monSpecies = &data->species;
        for (i = 0; i < enemies->datas[party_id].data.npoke; i++)
        {
            // Generate personality by seeding with a value based on the difficulty,
            // level, species, and opponent ID. Roll the RNG N times, where N is
            // the index of its trainer class. Finally, left shift the 16-bit
            // pseudorandom value and add the gender selector.
            // This guarantees that NPC trainers' Pokemon are generated in a
            // consistent manner between attempts.
            seed = monSpecies[i].difficulty + monSpecies[i].level + monSpecies[i].species + enemies->trainer_idxs[party_id];
            SetLCRNGSeed(seed);
            for (j = 0; j < enemies->datas[party_id].data.trainerClass; j++)
            {
                seed = LCRandom();
            }
            personality = (seed << 8);
            personality += pid_gender;

            // Difficulty is a number between 0 and 250 which directly corresponds
            // to the (uniform) IV spread of the generated Pokemon.
            iv = (u8)((monSpecies[i].difficulty * 31) / 255);
            CreateMon(pokemon, monSpecies[i].species, monSpecies[i].level, iv, 1, (s32)personality, 2, 0);

            // If you were treating the trainer type as a bitfield, you'd put the
            // checks for held item and moves here. You'd also treat the trpoke
            // data as a flat u16 array rather than an array of fixed-width structs.
            AddMonToParty(enemies->parties[party_id], pokemon);
        }
        break;
    }
    case TRTYPE_MON_MOVES:
    {
        monSpeciesMoves = &data->species_moves;
        for (i = 0; i < enemies->datas[party_id].data.npoke; i++)
        {
            seed = monSpeciesMoves[i].difficulty + monSpeciesMoves[i].level + monSpeciesMoves[i].species + enemies->trainer_idxs[party_id];
            SetLCRNGSeed(seed);
            for (j = 0; j < enemies->datas[party_id].data.trainerClass; j++)
            {
                seed = LCRandom();
            }
            personality = (seed << 8);
            personality += pid_gender;
            iv = (u8)((monSpeciesMoves[i].difficulty * 31) / 255);
            CreateMon(pokemon, monSpeciesMoves[i].species, monSpeciesMoves[i].level, iv, 1, (s32)personality, 2, 0);
            for (j = 0; j < 4; j++)
            {
                MonSetMoveInSlot(pokemon, monSpeciesMoves[i].moves[j], (u8)j);
            }
            AddMonToParty(enemies->parties[party_id], pokemon);
        }
        break;
    }
    case TRTYPE_MON_ITEM:
    {
        monSpeciesItem = &data->species_item;
        for (i = 0; i < enemies->datas[party_id].data.npoke; i++)
        {
            seed = monSpeciesItem[i].difficulty + monSpeciesItem[i].level + monSpeciesItem[i].species + enemies->trainer_idxs[party_id];
            SetLCRNGSeed(seed);
            for (j = 0; j < enemies->datas[party_id].data.trainerClass; j++)
            {
                seed = LCRandom();
            }
            personality = (seed << 8);
            personality += pid_gender;
            iv = (u8)((monSpeciesItem[i].difficulty * 31) / 255);
            CreateMon(pokemon, monSpeciesItem[i].species, monSpeciesItem[i].level, iv, 1, (s32)personality, 2, 0);
            SetMonData(pokemon, MON_DATA_HELD_ITEM, &monSpeciesItem[i].item);
            AddMonToParty(enemies->parties[party_id], pokemon);
        }
        break;
    }
    case TRTYPE_MON_ITEM_MOVES:
    {
        monSpeciesItemMoves = &data->species_item_moves;
        for (i = 0; i < enemies->datas[party_id].data.npoke; i++)
        {
            seed = monSpeciesItemMoves[i].difficulty + monSpeciesItemMoves[i].level + monSpeciesItemMoves[i].species + enemies->trainer_idxs[party_id];
            SetLCRNGSeed(seed);
            for (j = 0; j < enemies->datas[party_id].data.trainerClass; j++)
            {
                seed = LCRandom();
            }
            personality = (seed << 8);
            personality += pid_gender;
            iv = (u8)((monSpeciesItemMoves[i].difficulty * 31) / 255);
            CreateMon(pokemon, monSpeciesItemMoves[i].species, monSpeciesItemMoves[i].level, iv, 1, (s32)personality, 2, 0);
            SetMonData(pokemon, MON_DATA_HELD_ITEM, &monSpeciesItemMoves[i].item);
            for (j = 0; j < 4; j++)
            {
                MonSetMoveInSlot(pokemon, monSpeciesItemMoves[i].moves[j], (u8)j);
            }
            AddMonToParty(enemies->parties[party_id], pokemon);
        }
        break;
    }
    }
    FreeToHeap(data);
    FreeToHeap(pokemon);
    SetLCRNGSeed(seed_bak);
}
