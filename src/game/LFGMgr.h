/*
 * Copyright (C) 2011-2012 /dev/rsa for MangosR2 <http://github.com/MangosR2>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LFGMGR_H
#define _LFGMGR_H

#include "Common.h"
#include "ObjectGuid.h"
#include "Policies/Singleton.h"
#include <ace/RW_Thread_Mutex.h>
#include "LFG.h"
#include "Timer.h"

enum LFGenum
{
    LFG_TIME_ROLECHECK         = 2*MINUTE,
    LFG_TIME_BOOT              = 2*MINUTE,
    LFG_TIME_PROPOSAL          = 2*MINUTE,
    LFG_TIME_JOIN_WARNING      = 1*IN_MILLISECONDS,
    LFG_QUEUEUPDATE_INTERVAL   = 15*IN_MILLISECONDS,
    LFG_UPDATE_INTERVAL        = 1*IN_MILLISECONDS,
    LFR_UPDATE_INTERVAL        = 3*IN_MILLISECONDS,
    DEFAULT_LFG_DELAY          = 1,
    SHORT_LFG_DELAY            = 3,
    LONG_LFG_DELAY             = 10,
    LFG_SPELL_DUNGEON_COOLDOWN = 71328,
    LFG_SPELL_DUNGEON_DESERTER = 71041,
    LFG_SPELL_LUCK_OF_THE_DRAW = 72221,
};

enum LFGEventType
{
    LFG_EVENT_NONE                        = 0,
    LFG_EVENT_TELEPORT_PLAYER             = 1,
    LFG_EVENT_TELEPORT_GROUP              = 2,
};

class Group;
class Player;
class Map;
struct LFGDungeonExpansionEntry;
struct LFGProposal;

// Reward info
struct LFGReward
{
    uint32 maxLevel;
    struct
    {
        uint32 questId;
        uint32 variableMoney;
        uint32 variableXP;
    } reward[2];

    LFGReward(uint32 _maxLevel, uint32 firstQuest, uint32 firstVarMoney, uint32 firstVarXp, uint32 otherQuest, uint32 otherVarMoney, uint32 otherVarXp)
        : maxLevel(_maxLevel)
    {
        reward[0].questId = firstQuest;
        reward[0].variableMoney = firstVarMoney;
        reward[0].variableXP = firstVarXp;
        reward[1].questId = otherQuest;
        reward[1].variableMoney = otherVarMoney;
        reward[1].variableXP = otherVarXp;
    }
};

// Event manager
struct LFGEvent
{
    LFGEvent(LFGEventType _type, ObjectGuid _guid, uint8 _parm) :
            type(_type), guid(_guid), eventParm(_parm), executeTime(0) {};
    LFGEventType  type;
    ObjectGuid    guid;
    uint8         eventParm;
    time_t        executeTime;
    // helpers
    void          Start(uint32 delay) { executeTime = time_t(time(NULL) + delay); };
    bool          IsActive() { return ((executeTime > 0) && (time_t(time(NULL)) >= executeTime)); };
};

typedef std::multimap<uint32 /*dungeonId*/, LFGReward /*reward*/> LFGRewardMap;
typedef std::pair<LFGRewardMap::const_iterator, LFGRewardMap::const_iterator> LFGRewardMapBounds;
typedef std::map<LFGDungeonEntry const*, LFGQueueInfo> LFGQueueInfoMap;
typedef std::map<uint32/*dungeonID*/, LFGDungeonEntry const*> LFGDungeonMap;
typedef std::map<uint32/*ID*/, LFGProposal> LFGProposalMap;
typedef std::map<ObjectGuid, LFGRoleMask>  LFGRolesMap;
typedef std::map<LFGDungeonEntry const*, GuidSet> LFGSearchMap;
typedef std::list<LFGEvent> LFGEventList;

class LFGMgr
{
    public:
        LFGMgr();
        ~LFGMgr();

        // Update system
        void Update(uint32 diff);

        void TryCompleteGroups();
        bool TryAddMembersToGroup(Group* pGroup, GuidSet const* players);
        void CompleteGroup(Group* pGroup, GuidSet players, LFGDungeonEntry const* dungeon);
        bool TryCreateGroup();

        // Join system
        void Join(Player* pPlayer, LFGRoleMask roles, LFGDungeonSet const* dungeons, std::string comment);
        void JoinPlayer(Player* pPlayer, LFGRoleMask roles, LFGDungeonSet const* dungeons, LFGType type, std::string comment);
        void JoinGroup(Group* pGroup,Player* pLeader, LFGRoleMask leaderRoles, LFGDungeonSet const* dungeons, LFGType type, std::string leaderComment);
        void Leave(Player* pPlayer);
        void LeavePlayer(Player* pPlayer);
        void LeaveGroup(Group* pGroup);
        void ClearLFRList(Player* pPlayer);

        // Queue system
        void AddToQueue(ObjectGuid guid, LFGDungeonEntry const* pDungeon);
        void RemoveFromQueue(ObjectGuid guid);
        LFGQueueInfo* GetQueueInfo(LFGDungeonEntry const* pDungeon);
        GuidSet GetDungeonPlayerQueue(LFGDungeonEntry const* pDungeon, Team team = TEAM_NONE);
        GuidSet GetDungeonGroupQueue(LFGDungeonEntry const* pDungeon, Team team = TEAM_NONE);

        // reward system
        void LoadRewards();
        LFGReward const* GetRandomDungeonReward(LFGDungeonEntry const* pDungeon, Player* pPlayer);
        void SendLFGRewards(Group* pGroup);
        void SendLFGReward(Player* pPlayer, LFGDungeonEntry const* pDungeon, LFGDungeonEntry const* pDeclaredDungeon);
        void DungeonEncounterReached(Group* pGroup);

        // Proposal system
        uint32 CreateProposal(LFGDungeonEntry const* pDungeon, Group* pGroup, GuidSet playerGuids);
        bool SendProposal(uint32 uiID, ObjectGuid guid);
        LFGProposal* GetProposal(uint32 uiID);
        void RemoveFomProposal(Player* pDecliner, uint32 uiID);
        void RemoveProposal(uint32 uiID, bool bGroupCreateSuccess);
        void UpdateProposal(uint32 uiID, ObjectGuid guid, bool bAccept);
        void CleanupProposals();
        Player* LeaderElection(GuidSet* playerGuids);

        // boot vote system
        void OfferContinue(Group* pGroup);
        void InitBoot(Player* pKicker, ObjectGuid victimGuid, std::string reason);
        void UpdateBoot(Player* pPlayer, LFGAnswer answer);
        void CleanupBoots();

        // teleport system
        void Teleport(Group* pGroup, bool bOut = false);
        void Teleport(Player* pPlayer, bool bOut = false, bool bFromOpcode = false);

        // LFR extend system
        void UpdateLFRGroups();

        // Statistic system
        void UpdateQueueStatus();
        void SendStatistic();

        // Role check system
        void CleanupRoleChecks();
        void StartRoleCheck(Group* pGroup);
        void UpdateRoleCheck(Group* pGroup);
        bool CheckRoles(Group* pGroup, Player* pPlayer = NULL);
        bool CheckRoles(LFGRolesMap const* roleMap);
        bool RoleChanged(Player* pPlayer, LFGRoleMask roles);
        void SetGroupRoles(Group* pGroup, GuidSet const* players);
        void SetRoles(LFGRolesMap* roleMap);

        // Dungeon operations
        LFGDungeonEntry const* GetDungeon(uint32 dungeonID);
        LFGDungeonSet GetRandomDungeonsForPlayer(Player* pPlayer);

        // Group operations
        void AddMemberToLFDGroup(ObjectGuid guid);
        void RemoveMemberFromLFDGroup(Group* pGroup, ObjectGuid guid);
        void LoadLFDGroupPropertiesForPlayer(Player* pPlayer);

        // Dungeon expand operations
        LFGDungeonSet ExpandRandomDungeonsForGroup(LFGDungeonEntry const* randomDungeon, GuidSet playerGuids);

        // Checks
        LFGJoinResult GetPlayerJoinResult(Player* pPlayer, LFGDungeonSet const* dungeons);
        LFGJoinResult GetGroupJoinResult(Group* pGroup, LFGDungeonSet const* dungeons);

        // Player status
        LFGLockStatusType GetPlayerLockStatus(Player* pPlayer, LFGDungeonEntry const* pDungeon);
        LFGLockStatusType GetPlayerExpansionLockStatus(Player* pPlayer, LFGDungeonEntry const* pDungeon);
        LFGLockStatusType GetGroupLockStatus(Group* pGroup, LFGDungeonEntry const* pDungeon);
        LFGLockStatusMap GetPlayerLockMap(Player* pPlayer);

        // some static Helper Functions
        static LFGType GetAndCheckLFGType(LFGDungeonSet const* dungeons);
        static bool IsGroupCompleted(Group const* pGroup, uint8 uiAddMembers = 0);
        static LFGDungeonEntry const* SelectRandomDungeonFromList(LFGDungeonSet const* dungeons);
        static bool IsRandomDungeon(LFGDungeonEntry const* dungeon);
        // Social check system TODO: not the right place (maybe SocialMgr)
        static bool HasIgnoreState(Group* pGroup, ObjectGuid checkTarget);
        static bool HasIgnoreState(Player* pPlayer, ObjectGuid checkTarget);
        static bool IsInSameTeam(Group* pGroup, Player* pPlayer = NULL);
        static bool IsInSameTeam(Player* pPlayer1, Player* pPlayer2);


        // Sheduler
        void SheduleEvent();
        void AddEvent(ObjectGuid guid, LFGEventType type, time_t delay = DEFAULT_LFG_DELAY, uint8 param = 0);

        // Scripts
        void OnPlayerEnterMap(Player* pPlayer, Map* pMap);
        void OnPlayerLeaveMap(Player* pPlayer, Map* pMap);

        // multithread locking
        typedef   ACE_RW_Thread_Mutex          LockType;
        typedef   ACE_Read_Guard<LockType>     ReadGuard;
        typedef   ACE_Write_Guard<LockType>    WriteGuard;
        LockType& GetLock() { return i_lock; }

    private:
        uint32 GenerateProposalID();
        uint32          m_proposalID;
        LFGRewardMap    m_RewardMap;                        // Stores rewards for random dungeons
        LFGQueueInfoMap m_queueInfoMap;                     // Storage for queues
        LFGDungeonMap   m_dungeonMap;                       // sorted dungeon map
        LFGProposalMap  m_proposalMap;                      // Proposal store
        LFGEventList    m_eventList;                        // events storage
        GuidSet         m_GroupsWithBoot;

        IntervalTimer   m_LFGupdateTimer;                   // update timer for cleanup/statistic
        IntervalTimer   m_LFRupdateTimer;                   // update timer for LFR extend system
        IntervalTimer   m_LFGQueueUpdateTimer;              // update timer for statistic send

        LockType        i_lock;
};

#define sLFGMgr MaNGOS::Singleton<LFGMgr>::Instance()
#endif
