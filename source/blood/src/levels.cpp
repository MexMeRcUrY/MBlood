//-------------------------------------------------------------------------
/*
Copyright (C) 2010-2019 EDuke32 developers and contributors
Copyright (C) 2019 Nuke.YKT

This file is part of NBlood.

NBlood is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
//-------------------------------------------------------------------------
#include <stdlib.h>
#include <string.h>
#include "compat.h"
#include "common_game.h"

#include "asound.h"
#include "blood.h"
#include "config.h"
#include "credits.h"
#include "endgame.h"
#include "inifile.h"
#include "levels.h"
#include "loadsave.h"
#include "messages.h"
#include "network.h"
#include "screen.h"
#include "seq.h"
#include "sound.h"
#include "sfx.h"
#include "view.h"
#include "eventq.h"

GAMEOPTIONS gGameOptions;

GAMEOPTIONS gSingleGameOptions = {
    0,     // char nGameType;
    2,     // char nDifficulty;
    0,     // int nEpisode;
    0,     // int nLevel;
    "",    // char zLevelName[BMAX_PATH];
    "",    // char zLevelSong[BMAX_PATH];
    2,     // int nTrackNumber;
    "",    // char szSaveGameName[BMAX_PATH];
    "",    // char szUserGameName[BMAX_PATH];
    0,     // short nSaveGameSlot;
    0,     // int picEntry;
    0,     // unsigned int uMapCRC;
    1,     // char nMonsterSettings;
    kGameFlagNone, // int uGameFlags;
    kNetGameFlagNone, // int uNetGameFlags;
    0,     // char nWeaponSettings;
    0,     // char nItemSettings;
    0,     // char nRespawnSettings;
    2,     // char nTeamSettings;
    3600,  // int nMonsterRespawnTime;
    1800,  // int nWeaponRespawnTime;
    1800,  // int nItemRespawnTime;
    7200,  // int nSpecialRespawnTime;
    0,     // bool bQuadDamagePowerup;
    0,     // int nDamageInvul;
    0,     // int nExplosionBehavior;
    0,     // int nProjectileBehavior;
    0,     // bool bNapalmFalloff;
    0,     // int nEnemyBehavior;
    0,     // bool bEnemyRandomTNT;
    1,     // int nWeaponsVer;
    0,     // bool bSectorBehavior;
    0,     // char nHitscanProjectiles;
    0,     // char nRandomizerMode;
    "",    // char szRandomizerSeed[9];
    -1,    // int nRandomizerCheat;
    2,     // int nEnemyQuantity;
    2,     // int nEnemyHealth;
    0,     // int nEnemySpeed;
    0,     // bool bEnemyShuffle;
    0,     // bool bPitchforkOnly;
    0,     // bool bPermaDeath;
    0,     // bool bFriendlyFire;
    1,     // char nKeySettings;
    0,     // char bItemWeaponSettings;
    1,     // char bAutoTeams;
    0,     // char nSpawnProtection;
    0,     // char nSpawnWeapon;
    BANNED_NONE, // unsigned int uSpriteBannedFlags;
    "",    // char szUserMap[BMAX_PATH];
};

EPISODEINFO gEpisodeInfo[kMaxEpisodes+1];

int gSkill = 2;
int gEpisodeCount;
int gNextLevel;
bool gGameStarted;

int gLevelTime;

char BloodIniFile[BMAX_PATH] = "BLOOD.INI";
char BloodIniPre[BMAX_PATH];
bool bINIOverride = false;
IniFile *BloodINI;


void levelInitINI(const char *pzIni)
{
    int fp = kopen4loadfrommod(pzIni, 0);
    if (fp < 0)
        ThrowError("Initialization: %s does not exist", pzIni);
    kclose(fp);
    BloodINI = new IniFile(pzIni);
    Bstrncpy(BloodIniFile, pzIni, BMAX_PATH);
    Bstrncpy(BloodIniPre, pzIni, BMAX_PATH);
    ChangeExtension(BloodIniPre, "");
}


void levelOverrideINI(const char *pzIni)
{
    bINIOverride = true;
    strcpy(BloodIniFile, pzIni);
}
///////
//Cutscene / cinematic
void showWaitingScreenForCoop(void)
{
    if (gGameOptions.nGameType == 1)
    {
        viewLoadingScreen(2518, "Network Game", "Playing Cinematic", "Waiting for other player(s) to skip...");
        videoNextPage();
        netWaitForEveryone(0);
    }
}

bool playCinematic(const char* episodeCutscene, const char* cutsceneWavPath, int cutsceneWavRsrcID, CinematicFormats &formatFound) {
    // get filename
    char* fullPath = new char[MAX_PATH];
    memset(fullPath, 0, MAX_PATH);
    char* cs = new char[strlen(episodeCutscene)];
    strcpy(cs, episodeCutscene);
    //uniform path in case multiplatform
    for (int i = 0; i < strlen(cs); i++)
    {
        if (cs[i] == '/')
        {
            cs[i] = '\\';
        }
    }
    //file name with extension
    char* fileName = strrchr(cs, '\\');
    //if fail to substract subdirectories use the file name as it comes.
    if (fileName == NULL)
    {
        fileName = new char[strlen(episodeCutscene)];
        strcpy(fileName, episodeCutscene);
    }
    //remove first back slash
    if (fileName != NULL && fileName[0] == '\\')
    {
        memmove(&fileName[0], &fileName[0 + 1], strlen(fileName));
    }
    //file name without extension
    char* lastExt;
    lastExt = strrchr(fileName, '.');
    if (lastExt == NULL) return false;
    if (lastExt != NULL)
        *lastExt = '\0';

    for (int i = 0; i < 4; i++) //loop the 3 options: local path, auto load path, game dir path
    {
        const char* CUR_DIR;
        switch (i)
        {
        case 0:
            CUR_DIR = "./Movie/";
            break;
        case 1:
            CUR_DIR = "./autoload/Movie/";
            break;
        case 2:
            CUR_DIR = G_GetGamePath(Games_t::kGame_Blood);
            break;
        case 3:
            char mov[MAX_PATH];
            memset(mov, 0, MAX_PATH);
            CUR_DIR = G_GetGamePath(Games_t::kGame_Blood);
            strcat(mov, CUR_DIR);
            strcat(mov, "/Movie");
            CUR_DIR = mov;
            break;

        }

        bool bkpPathsearchmode = pathsearchmode;
        pathsearchmode = 1; // makes the klistpath only look into the passed directory.
        BUILDVFS_FIND_REC* ogvFilesList = klistpath(CUR_DIR, "*.ogv", BUILDVFS_FIND_FILE);
        pathsearchmode = bkpPathsearchmode;
        pINIChain = NULL;
        char* ogvFile = new char[strlen(fileName) + 4];
        strcpy(ogvFile, fileName);
        strcat(ogvFile, ".ogv");
        for (auto pIter = ogvFilesList; pIter; pIter = pIter->next)
        {
            if (!Bstrncasecmp(ogvFile, pIter->name, BMAX_PATH))
            {
                strcat(fullPath, CUR_DIR);
                strcat(fullPath, ogvFile);
                formatFound = CinematicFormats::OGV;
            }

        }

        BUILDVFS_FIND_REC* smkFilesList = klistpath(CUR_DIR, "*.SMK", BUILDVFS_FIND_FILE);

        pINIChain = NULL;
        char* smkFile = new char[strlen(fileName) + 4];
        strcpy(smkFile, fileName);
        strcat(smkFile, ".SMK");
        for (auto pIter = smkFilesList; pIter; pIter = pIter->next)
        {
            if (!Bstrncasecmp(smkFile, pIter->name, BMAX_PATH))
            {
                strcat(fullPath, CUR_DIR);
                strcat(fullPath, smkFile);
                formatFound = SMK;
            }

        }

        //try movies in game Data path
        bool moviePlayed = false;
        char* smkMovieFullPath = new char[MAX_PATH];
        memset(smkMovieFullPath, 0, MAX_PATH);
        char* ogvMovieFullPath = new char[MAX_PATH];
        memset(ogvMovieFullPath, 0, MAX_PATH);
        

        switch (formatFound)
        {
        case CinematicFormats::SMK:
            strcpy(smkMovieFullPath, fullPath);
            break;
        case CinematicFormats::OGV:
            strcpy(ogvMovieFullPath, fullPath);
            break;
        }
        if (!moviePlayed)
        {
            moviePlayed = credPlayTheora(ogvMovieFullPath);
        }
        if (!moviePlayed)
        {
            moviePlayed = credPlaySmk(smkMovieFullPath, cutsceneWavPath, cutsceneWavRsrcID);
        }

        if (moviePlayed)
        {
            showWaitingScreenForCoop();
            return true;
        }
    }
    return false; 
   
}
///////
void playCutscene(int nEpisode, int nSceneType)
{
    gGameOptions.uGameFlags &= ~kGameFlagPlayIntro;
    sndStopSong();
    sndKillAllSounds();
    sfxKillAllSounds();
    ambKillAll();
    seqKillAll();
///////    
    ControlInfo info;
    //Cutscene / cinematic
    if (!gCutScenes)
    {
        scrSetDac();
        viewResizeView(gViewSize);
        credReset();
        scrSetDac();
        CONTROL_GetInput(&info); // clear mouse and all input after cutscene has finished playing
        ctrlClearAllInput();
        return;
    }
    
    EPISODEINFO* pEpisode = &gEpisodeInfo[nEpisode];
    char* smkMovieFullPath = new char[MAX_PATH];
    memset(smkMovieFullPath, 0, MAX_PATH);
    char* ogvMovieFullPath = new char[MAX_PATH];
    memset(ogvMovieFullPath, 0, MAX_PATH);
    char* rootDirMoviePath = new char[MAX_PATH];
    char *rootDirPath = new char[MAX_PATH];
    char* cutscenePath = new char[MAX_PATH];
    char* cutsceneWavPath = new char[MAX_PATH];
    int   cutsceneWavRsrcID =0;

    switch (nSceneType)
    {
    case kGameFlagPlayIntro:
        
            cutscenePath = pEpisode->cutsceneASmkPath;
            cutsceneWavPath = pEpisode->cutsceneAWavPath;
            cutsceneWavRsrcID = pEpisode->cutsceneAWavRsrcID;
            break;
    case kGameFlagEnding:
    case kGameFlagPlayOutro:
        
            cutscenePath = pEpisode->cutsceneBSmkPath;
            cutsceneWavPath = pEpisode->cutsceneBWavPath;
            cutsceneWavRsrcID = pEpisode->cutsceneBWavRsrcID;
            break;
    case kGameFlagContinuing:
        //TODO:
        break;

    }
    char* fullpathFound = new char[MAX_PATH];
    memset(fullpathFound, 0, MAX_PATH);
    CinematicFormats formatFound = CINEMATICEXTCOUNT;
    playCinematic(cutscenePath, cutsceneWavPath, cutsceneWavRsrcID, formatFound);

   
    scrSetDac();
    viewResizeView(gViewSize);
    credReset();
    scrSetDac();
    CONTROL_GetInput(&info); // clear mouse and all input after cutscene has finished playing
    ctrlClearAllInput();
    ///////
}

void levelClearSecrets(void)
{
    gSecretMgr.Clear();
}

void levelSetupSecret(int nCount)
{
    gSecretMgr.SetCount(nCount);
}

void levelTriggerSecret(int nSecret)
{
    gSecretMgr.Found(nSecret);
}

void CheckSectionAbend(const char *pzSection)
{
    if (!pzSection || !BloodINI->SectionExists(pzSection))
        ThrowError("Section [%s] expected in BLOOD.INI", pzSection);
}

void CheckKeyAbend(const char *pzSection, const char *pzKey)
{
    dassert(pzSection != NULL);

    if (!pzKey || !BloodINI->KeyExists(pzSection, pzKey))
        ThrowError("Key %s expected in section [%s] of BLOOD.INI", pzKey, pzSection);
}

LEVELINFO * levelGetInfoPtr(int nEpisode, int nLevel)
{
    dassert(nEpisode >= 0 && nEpisode < gEpisodeCount);
    EPISODEINFO *pEpisodeInfo = &gEpisodeInfo[nEpisode];
    dassert(nLevel >= 0 && nLevel < pEpisodeInfo->nLevels);
    return &pEpisodeInfo->levelsInfo[nLevel];
}

char * levelGetFilename(int nEpisode, int nLevel)
{
    dassert(nEpisode >= 0 && nEpisode < gEpisodeCount);
    EPISODEINFO *pEpisodeInfo = &gEpisodeInfo[nEpisode];
    dassert(nLevel >= 0 && nLevel < pEpisodeInfo->nLevels);
    return pEpisodeInfo->levelsInfo[nLevel].Filename;
}

char * levelGetMessage(int nMessage)
{
    int nEpisode = gGameOptions.nEpisode;
    int nLevel = gGameOptions.nLevel;
    dassert(nMessage < kMaxMessages);
    char *pMessage = gEpisodeInfo[nEpisode].levelsInfo[nLevel].Messages[nMessage];
    if (*pMessage == 0)
        return NULL;
    if (VanillaMode() && (Bstrlen(pMessage) > 63)) // don't print incompatible messages for vanilla mode
    {
        OSD_Printf("> Warning: Level E%dM%d message #%d longer than 63 characters (incompatible with vanilla mode).\n", nEpisode, nLevel, nMessage);
        pMessage[63] = '\0'; // terminate message to emulate vanilla 1.21 string size
    }
    return pMessage;
}

char * levelGetTitle(void)
{
    int nEpisode = gGameOptions.nEpisode;
    int nLevel = gGameOptions.nLevel;
    char *pTitle = gEpisodeInfo[nEpisode].levelsInfo[nLevel].Title;
    if (*pTitle == 0)
        return NULL;
    for (int i = Bstrlen(pTitle)-1; i > 0; i--)
    {
        if ((pTitle[i] == '/') || (pTitle[i] == '\\'))
            return &pTitle[i+1];
    }
    return pTitle;
}

char * levelGetAuthor(void)
{
    int nEpisode = gGameOptions.nEpisode;
    int nLevel = gGameOptions.nLevel;
    char *pAuthor = gEpisodeInfo[nEpisode].levelsInfo[nLevel].Author;
    if (*pAuthor == 0)
        return NULL;
    return pAuthor;
}

void levelSetupOptions(int nEpisode, int nLevel)
{
    gGameOptions.nEpisode = nEpisode;
    gGameOptions.nLevel = nLevel;
    strcpy(gGameOptions.zLevelName, gEpisodeInfo[nEpisode].levelsInfo[nLevel].Filename);
    gGameOptions.uMapCRC = dbReadMapCRC(gGameOptions.zLevelName);
    // strcpy(gGameOptions.zLevelSong, gEpisodeInfo[nEpisode].at28[nLevel].atd0);
    gGameOptions.nTrackNumber = gEpisodeInfo[nEpisode].levelsInfo[nLevel].SongId;
}

void levelLoadMapInfo(IniFile *pIni, LEVELINFO *pLevelInfo, const char *pzSection)
{
    char buffer[16];
    strncpy(pLevelInfo->Title, pIni->GetKeyString(pzSection, "Title", pLevelInfo->Filename), 31);
    strncpy(pLevelInfo->Author, pIni->GetKeyString(pzSection, "Author", ""), 31);
    strncpy(pLevelInfo->Song, pIni->GetKeyString(pzSection, "Song", ""), BMAX_PATH);
    pLevelInfo->SongId = pIni->GetKeyInt(pzSection, "Track", -1);
    pLevelInfo->EndingA = pIni->GetKeyInt(pzSection, "EndingA", -1);
    pLevelInfo->EndingB = pIni->GetKeyInt(pzSection, "EndingB", -1);
    pLevelInfo->Fog = pIni->GetKeyInt(pzSection, "Fog", -0);
    pLevelInfo->Weather = pIni->GetKeyInt(pzSection, "Weather", -0);
    for (int i = 0; i < kMaxMessages; i++)
    {
        sprintf(buffer, "Message%d", i+1);
        strncpy(pLevelInfo->Messages[i], pIni->GetKeyString(pzSection, buffer, ""), 127);
    }
}

extern void MenuSetupEpisodeInfo(void);

void levelLoadDefaults(void)
{
    char buffer[64];
    char buffer2[16];
    levelInitINI(pINISelected->zName);
    memset(gEpisodeInfo, 0, sizeof(gEpisodeInfo));
    strncpy(gEpisodeInfo[MUS_INTRO/kMaxLevels].levelsInfo[MUS_INTRO%kMaxLevels].Song, "PESTIS", BMAX_PATH);
    int i;
    for (i = 0; i < kMaxEpisodes; i++)
    {
        sprintf(buffer, "Episode%d", i+1);
        if (!BloodINI->SectionExists(buffer))
            break;
        EPISODEINFO *pEpisodeInfo = &gEpisodeInfo[i];
        strncpy(pEpisodeInfo->title, BloodINI->GetKeyString(buffer, "Title", buffer), 31);
        strncpy(pEpisodeInfo->cutsceneASmkPath, BloodINI->GetKeyString(buffer, "CutSceneA", ""), BMAX_PATH);
        pEpisodeInfo->cutsceneAWavRsrcID = BloodINI->GetKeyInt(buffer, "CutWavA", -1);
        if (pEpisodeInfo->cutsceneAWavRsrcID == 0)
            strncpy(pEpisodeInfo->cutsceneAWavPath, BloodINI->GetKeyString(buffer, "CutWavA", ""), BMAX_PATH);
        else
            pEpisodeInfo->cutsceneAWavPath[0] = 0;
        strncpy(pEpisodeInfo->cutsceneBSmkPath, BloodINI->GetKeyString(buffer, "CutSceneB", ""), BMAX_PATH);
        pEpisodeInfo->cutsceneBWavRsrcID = BloodINI->GetKeyInt(buffer, "CutWavB", -1);
        if (pEpisodeInfo->cutsceneBWavRsrcID == 0)
            strncpy(pEpisodeInfo->cutsceneBWavPath, BloodINI->GetKeyString(buffer, "CutWavB", ""), BMAX_PATH);
        else
            pEpisodeInfo->cutsceneBWavPath[0] = 0;

        pEpisodeInfo->bloodbath = BloodINI->GetKeyInt(buffer, "BloodBathOnly", 0);
        pEpisodeInfo->cutALevel = BloodINI->GetKeyInt(buffer, "CutSceneALevel", 0);
        if (pEpisodeInfo->cutALevel > 0)
            pEpisodeInfo->cutALevel--;
        int j;
        for (j = 0; j < kMaxLevels; j++)
        {
            LEVELINFO *pLevelInfo = &pEpisodeInfo->levelsInfo[j];
            sprintf(buffer2, "Map%d", j+1);
            if (!BloodINI->KeyExists(buffer, buffer2))
                break;
            const char *pMap = BloodINI->GetKeyString(buffer, buffer2, NULL);
            CheckSectionAbend(pMap);
            strncpy(pLevelInfo->Filename, pMap, BMAX_PATH);
            levelLoadMapInfo(BloodINI, pLevelInfo, pMap);
        }
        pEpisodeInfo->nLevels = j;
    }
    gEpisodeCount = i;
    MenuSetupEpisodeInfo();
}

void levelAddUserMap(const char *pzMap)
{
    char buffer[BMAX_PATH];
    //strcpy(buffer, g_modDir);
    strncpy(buffer, pzMap, BMAX_PATH);
    ChangeExtension(buffer, ".DEF");

    IniFile UserINI(buffer);
    int nEpisode = ClipRange(UserINI.GetKeyInt(NULL, "Episode", 0), 0, 5);
    EPISODEINFO *pEpisodeInfo = &gEpisodeInfo[nEpisode];
    int nLevel = ClipRange(UserINI.GetKeyInt(NULL, "Level", pEpisodeInfo->nLevels), 0, 15);
    if (nLevel >= pEpisodeInfo->nLevels)
    {
        if (pEpisodeInfo->nLevels == 0)
        {
            gEpisodeCount++;
            sprintf(pEpisodeInfo->title, "Episode %d", nEpisode);
        }
        nLevel = pEpisodeInfo->nLevels++;
    }
    LEVELINFO *pLevelInfo = &pEpisodeInfo->levelsInfo[nLevel];
    ChangeExtension(buffer, "");
    strncpy(pLevelInfo->Filename, buffer, BMAX_PATH);
    levelLoadMapInfo(&UserINI, pLevelInfo, NULL);
    gGameOptions.nEpisode = nEpisode;
    gGameOptions.nLevel = nLevel;
    gGameOptions.uMapCRC = dbReadMapCRC(pLevelInfo->Filename);
    strcpy(gGameOptions.zLevelName, pLevelInfo->Filename);
    MenuSetupEpisodeInfo();
}

void levelGetNextLevels(int nEpisode, int nLevel, int *pnEndingA, int *pnEndingB)
{
    dassert(pnEndingA != NULL && pnEndingB != NULL);
    LEVELINFO *pLevelInfo = &gEpisodeInfo[nEpisode].levelsInfo[nLevel];
    int nEndingA = pLevelInfo->EndingA;
    if (nEndingA >= 0)
        nEndingA--;
    int nEndingB = pLevelInfo->EndingB;
    if (nEndingB >= 0)
        nEndingB--;
    *pnEndingA = nEndingA;
    *pnEndingB = nEndingB;
}

void levelEndLevel(int nExitType)
{
    int nEndingA, nEndingB;
    EPISODEINFO *pEpisodeInfo = &gEpisodeInfo[gGameOptions.nEpisode];
    gGameOptions.uGameFlags |= kGameFlagContinuing;
    levelGetNextLevels(gGameOptions.nEpisode, gGameOptions.nLevel, &nEndingA, &nEndingB);
    switch (nExitType)
    {
    case kLevelExitNormal:
        if (nEndingA == -1) // no more levels (reached end of episode), trigger credits
        {
            if (pEpisodeInfo->cutsceneBSmkPath[0]) // if ending cutscene file present, set to play movie
                gGameOptions.uGameFlags |= kGameFlagPlayOutro;
            gGameOptions.nLevel = 0;
            gGameOptions.uGameFlags |= kGameFlagEnding;
        }
        else
            gNextLevel = nEndingA;
        break;
    case kLevelExitSecret:
        if (nEndingB == -1) // no more levels (reached end of episode), trigger credits
        {
            if (gGameOptions.nEpisode + 1 < gEpisodeCount)
            {
                if (pEpisodeInfo->cutsceneBSmkPath[0]) // if ending cutscene file present, set to play movie
                    gGameOptions.uGameFlags |= kGameFlagPlayOutro;
                gGameOptions.nLevel = 0;
                gGameOptions.uGameFlags |= kGameFlagEnding;
            }
            else
            {
                gGameOptions.nLevel = 0;
                gGameOptions.uGameFlags |= kGameFlagContinuing;
            }
        }
        else
            gNextLevel = nEndingB;
        break;
    }
}

void levelRestart(void)
{
    levelSetupOptions(gGameOptions.nEpisode, gGameOptions.nLevel);
    gStartNewGame = true;
    gAutosaveInCurLevel = false;
}

int levelGetMusicIdx(const char *str)
{
    int32_t lev, ep;
    signed char b1, b2;

    int numMatches = sscanf(str, "%c%d%c%d", &b1, &ep, &b2, &lev);

    if (numMatches != 4 || Btoupper(b1) != 'E' || Btoupper(b2) != 'L')
        return -1;

    if ((unsigned)--lev >= kMaxLevels || (unsigned)--ep >= kMaxEpisodes)
        return -2;

    return (ep * kMaxLevels) + lev;
}

bool levelTryPlayMusic(int nEpisode, int nLevel, bool bSetLevelSong)
{
    char buffer[BMAX_PATH];
    if (CDAudioToggle && gEpisodeInfo[nEpisode].levelsInfo[nLevel].SongId > 0 && (!CDAudioFallback || gEpisodeInfo[nEpisode].levelsInfo[nLevel].Song[0] == '\0'))
        snprintf(buffer, BMAX_PATH, "blood%02i.ogg", gEpisodeInfo[nEpisode].levelsInfo[nLevel].SongId);
    else
        strncpy(buffer, gEpisodeInfo[nEpisode].levelsInfo[nLevel].Song, BMAX_PATH);
    bool bReturn = !!sndPlaySong(buffer, true);
    if (!bReturn || bSetLevelSong)
        strncpy(gGameOptions.zLevelSong, buffer, BMAX_PATH);
    return bReturn;
}

void levelTryPlayMusicOrNothing(int nEpisode, int nLevel)
{
    if (levelTryPlayMusic(nEpisode, nLevel, true))
        sndStopSong();
}

class LevelsLoadSave : public LoadSave
{
    virtual void Load(void);
    virtual void Save(void);
};


static LevelsLoadSave *myLoadSave;

void LevelsLoadSave::Load(void)
{
    Read(&gNextLevel, sizeof(gNextLevel));
    Read(&gGameOptions, sizeof(gGameOptions));
    Read(&gGameStarted, sizeof(gGameStarted));
}

void LevelsLoadSave::Save(void)
{
    Write(&gNextLevel, sizeof(gNextLevel));
    Write(&gGameOptions, sizeof(gGameOptions));
    Write(&gGameStarted, sizeof(gGameStarted));
}

void LevelsLoadSaveConstruct(void)
{
    myLoadSave = new LevelsLoadSave();
}

