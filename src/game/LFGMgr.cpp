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

#include "Common.h"
#include "Policies/SingletonImp.h"
#include "SharedDefines.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "SocialMgr.h"
#include "LFGMgr.h"
#include "World.h"
#include "Group.h"
#include "Player.h"

#include <limits>

INSTANTIATE_SINGLETON_1(LFGMgr);

LFGMgr::LFGMgr()
{
    for (uint32 i = 0; i < sLFGDungeonStore.GetNumRows(); ++i)
    {
        if (LFGDungeonEntry const* dungeon = sLFGDungeonStore.LookupEntry(i))
        {
            m_dungeonMap.insert(std::make_pair(dungeon->ID, dungeon));
        }
    }

    m_proposalID   = 1;
    m_LFGupdateTimer.SetInterval(LFG_UPDATE_INTERVAL);
    m_LFGupdateTimer.Reset();
    m_LFRupdateTimer.SetInterval(LFR_UPDATE_INTERVAL);
    m_LFRupdateTimer.Reset();
    m_LFGQueueUpdateTimer.SetInterval(LFG_QUEUEUPDATE_INTERVAL);
    m_LFGQueueUpdateTimer.Reset();
}

LFGMgr::~LFGMgr()
{
    m_RewardMap.clear();
    m_queueInfoMap.clear();
    m_dungeonMap.clear();
    m_proposalMap.clear();
    m_eventList.clear();
}

void LFGMgr::Update(uint32 uiDiff)
{

    SheduleEvent();

    if (m_queueInfoMap.empty())
        return;

    m_LFGupdateTimer.Update(uiDiff);
    m_LFRupdateTimer.Update(uiDiff);
    m_LFGQueueUpdateTimer.Update(uiDiff);

    if (m_LFGupdateTimer.Passed())
    {
        CleanupProposals();
        CleanupRoleChecks();
        CleanupBoots();
        TryCompleteGroups();
        TryCreateGroup();
        m_LFGupdateTimer.Reset();
    }

    if (m_LFRupdateTimer.Passed())
    {
        UpdateLFRGroups();
        m_LFRupdateTimer.Reset();
    }

    if (m_LFGQueueUpdateTimer.Passed())
    {
        UpdateQueueStatus();
        SendStatistic();
        m_LFGQueueUpdateTimer.Reset();
    }
}

void LFGMgr::LoadRewards()
{
   // (c) TrinityCore, 2010. Rewrited for MaNGOS by /dev/rsa

    m_RewardMap.clear();

    uint32 count = 0;
    // ORDER BY is very important for GetRandomDungeonReward!
    QueryResult* result = WorldDatabase.Query("SELECT dungeonId, maxLevel, firstQuestId, firstMoneyVar, firstXPVar, otherQuestId, otherMoneyVar, otherXPVar FROM lfg_dungeon_rewards ORDER BY dungeonId, maxLevel ASC");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outErrorDb(">> Loaded 0 LFG dungeon rewards. DB table `lfg_dungeon_rewards` is empty!");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    Field* fields = NULL;
    do
    {
        bar.step();
        fields = result->Fetch();
        uint32 dungeonId = fields[0].GetUInt32();
        uint32 maxLevel = fields[1].GetUInt8();
        uint32 firstQuestId = fields[2].GetUInt32();
        uint32 firstMoneyVar = fields[3].GetUInt32();
        uint32 firstXPVar = fields[4].GetUInt32();
        uint32 otherQuestId = fields[5].GetUInt32();
        uint32 otherMoneyVar = fields[6].GetUInt32();
        uint32 otherXPVar = fields[7].GetUInt32();

        if (!sLFGDungeonStore.LookupEntry(dungeonId))
        {
            sLog.outErrorDb("LFGMgr: Dungeon %u specified in table `lfg_dungeon_rewards` does not exist!", dungeonId);
            continue;
        }

        if (!maxLevel || maxLevel > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        {
            sLog.outErrorDb("LFGMgr: Level %u specified for dungeon %u in table `lfg_dungeon_rewards` can never be reached!", maxLevel, dungeonId);
            maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
        }

        if (firstQuestId && !sObjectMgr.GetQuestTemplate(firstQuestId))
        {
            sLog.outErrorDb("LFGMgr: First quest %u specified for dungeon %u in table `lfg_dungeon_rewards` does not exist!", firstQuestId, dungeonId);
            firstQuestId = 0;
        }

        if (otherQuestId && !sObjectMgr.GetQuestTemplate(otherQuestId))
        {
            sLog.outErrorDb("LFGMgr: Other quest %u specified for dungeon %u in table `lfg_dungeon_rewards` does not exist!", otherQuestId, dungeonId);
            otherQuestId = 0;
        }
        LFGReward reward = LFGReward(maxLevel, firstQuestId, firstMoneyVar, firstXPVar, otherQuestId, otherMoneyVar, otherXPVar);
        m_RewardMap.insert(LFGRewardMap::value_type(dungeonId, reward));
        ++count;
    }
    while (result->NextRow());

    sLog.outString();
    sLog.outString(">> Loaded %u LFG dungeon rewards.", count);
}

LFGReward const* LFGMgr::GetRandomDungeonReward(LFGDungeonEntry const* dungeon, Player* pPlayer)
{
    LFGReward const* rew = NULL;
    if (pPlayer)
    {
        LFGRewardMapBounds bounds = m_RewardMap.equal_range(dungeon->ID);
        for (LFGRewardMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
        {
            // Difficulty check TODO
            rew = &itr->second;
            // ordered properly at loading
            if (itr->second.maxLevel >= pPlayer->getLevel())
                break;
        }
    }
    return rew;
}

void LFGMgr::Join(Player* pPlayer, LFGRoleMask roles, LFGDungeonSet const* dungeons, std::string comment)
{
    if (!pPlayer)
        return;

    if (!sWorld.getConfig(CONFIG_BOOL_LFG_ENABLE) && !sWorld.getConfig(CONFIG_BOOL_LFR_ENABLE))
        return;

    if (roles == LFG_ROLE_MASK_NONE)
    {
        DEBUG_LOG("LFGMgr::Join:Error: %u has no roles! Aborting", pPlayer->GetObjectGuid().GetCounter());
        pPlayer->GetSession()->SendLfgJoinResult(ERR_LFG_ROLE_CHECK_FAILED2);
        return;
    }

    if (dungeons->empty())
    {
        DEBUG_LOG("LFGMgr::Join: %u trying to join without Dungeons. Aborting.", pPlayer->GetObjectGuid().GetCounter());
        pPlayer->GetSession()->SendLfgJoinResult(ERR_LFG_INVALID_SLOT);
        return;
    }

    LFGType type = LFGMgr::GetAndCheckLFGType(dungeons);

    if (type == LFG_TYPE_NONE)
    {
        DEBUG_LOG("LFGMgr::Join: %u trying to join different dungeon type. Aborting", pPlayer->GetObjectGuid().GetCounter());
        pPlayer->GetSession()->SendLfgJoinResult(ERR_LFG_INVALID_SLOT);
        return;
    }

    if (Group* pGroup = pPlayer->GetGroup())
    {
        // check for Interface correctness, we can only join, if we are groupleader
        if (pPlayer->GetObjectGuid() != pGroup->GetLeaderGuid())
        {
            DEBUG_LOG("LFGMgr::Join: %u trying to join with group, but not group leader. Aborting.", pPlayer->GetObjectGuid().GetCounter());
            pPlayer->GetSession()->SendLfgJoinResult(ERR_LFG_NO_SLOTS_PLAYER);
            return;
        }
        JoinGroup(pGroup, pPlayer, roles, dungeons, type, comment);
    }
    else
    {
        JoinPlayer(pPlayer, roles, dungeons, type, comment);
    }
}

void LFGMgr::JoinPlayer(Player* pPlayer, LFGRoleMask roles, LFGDungeonSet const* dungeons, LFGType type, std::string comment)
{
    ObjectGuid guid = pPlayer->GetObjectGuid();

    LFGJoinResult result = GetPlayerJoinResult(pPlayer, dungeons);
    if (result != ERR_LFG_OK)
    {
        DEBUG_LOG("LFGMgr::JoinPlayer: Player %u joining without members. result: %u - Aborting", guid.GetCounter(), result);
        pPlayer->GetSession()->SendLfgJoinResult(result);
        return;
    }

    // All Okay - Joining process
    pPlayer->GetLFGPlayerState()->SetJoined();

    pPlayer->GetLFGPlayerState()->SetDungeons(*dungeons);
    pPlayer->GetLFGPlayerState()->SetType(type);
    pPlayer->GetLFGPlayerState()->SetRoles(roles);
    pPlayer->GetLFGPlayerState()->SetComment(comment);

    RemoveFromQueue(guid);
    for (LFGDungeonSet::const_iterator itr = dungeons->begin(); itr != dungeons->end(); ++itr)
        AddToQueue(guid, *itr);
    pPlayer->GetLFGPlayerState()->SetState((type == LFG_TYPE_RAID) ? LFG_STATE_LFR : LFG_STATE_QUEUED);
    pPlayer->GetSession()->SendLfgJoinResult(result, LFG_ROLECHECK_NONE);
    pPlayer->GetSession()->SendLfgUpdatePlayer(LFG_UPDATETYPE_JOIN_PROPOSAL,type);
    pPlayer->GetSession()->SendLfgUpdateSearch(true);
}

void LFGMgr::JoinGroup(Group* pGroup,Player* pLeader, LFGRoleMask leaderRoles, LFGDungeonSet const* dungeons, LFGType type, std::string leaderComment)
{
    if (!pGroup || !pLeader)
        return;

    ObjectGuid guid = pGroup->GetObjectGuid();

    LFGJoinResult result = GetGroupJoinResult(pGroup, dungeons);
    if (result != ERR_LFG_OK)
    {
        DEBUG_LOG("LFGMgr::JoinGroup: Group %u joining with %u members. result: %u - Abort", guid.GetCounter(), pGroup->GetMembersCount(), result);
        pLeader->GetSession()->SendLfgJoinResult(result);
        return;
    }

    // all Okay - Joining process as group
    pLeader->GetLFGPlayerState()->SetJoined();

    pLeader->GetLFGPlayerState()->SetDungeons(*dungeons);
    pLeader->GetLFGPlayerState()->SetRoles(leaderRoles);
    pLeader->GetLFGPlayerState()->SetComment(leaderComment);

    LFGGroupState* groupState = pGroup->GetLFGGroupState();

    // if we are already in dungeon, then we must not change type and dungeon, because the "offer continue" select only the dungeon where the group is
    if (!groupState->GetChosenDungeon())
    {
        DEBUG_LOG("We are not in Offer continue, therefore we change the type %u and the dungeons.", type);
        pGroup->GetLFGGroupState()->SetDungeons(*dungeons);
        pGroup->GetLFGGroupState()->SetType(type);
    }
    else
    {
        dungeons = pGroup->GetLFGGroupState()->GetDungeons();
        DEBUG_LOG("We are in Offer continue, therefore we doesn't change the type (oldtype %u) and the dungeons.", pGroup->GetLFGGroupState()->GetType());
    }

    switch (groupState->GetState())
    {
        case LFG_STATE_NONE:
        case LFG_STATE_FINISHED_DUNGEON:
        {
            RemoveFromQueue(guid);
            for (LFGDungeonSet::const_iterator itr = dungeons->begin(); itr != dungeons->end(); ++itr)
                AddToQueue(guid, *itr);
            groupState->SetState((type == LFG_TYPE_RAID) ? LFG_STATE_LFR : LFG_STATE_LFG);
            groupState->SetStatus(LFG_STATUS_NOT_SAVED);

            for (GroupReference *itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* pGroupMember = itr->getSource();
                if (pGroupMember && pGroupMember->IsInWorld())
                {
                    RemoveFromQueue(pGroupMember->GetObjectGuid());
                    if(sWorld.getConfig(CONFIG_BOOL_RESTRICTED_LFG_CHANNEL))
                        pGroupMember->JoinLFGChannel();
                    pGroupMember->GetSession()->SendLfgUpdateParty(LFG_UPDATETYPE_JOIN_PROPOSAL, type);
                    pGroupMember->GetLFGPlayerState()->SetState((type == LFG_TYPE_RAID) ? LFG_STATE_LFR : LFG_STATE_LFG);
                }
            }
            if (type == LFG_TYPE_RAID && sWorld.getConfig(CONFIG_BOOL_LFR_EXTEND))
            {
                pGroup->ConvertToLFG(type);
            }
            else
            {
                StartRoleCheck(pGroup);
            }
            break;
        }
        // re-adding to queue
        case LFG_STATE_IN_DUNGEON:
        {
            RemoveFromQueue(guid);
            for (LFGDungeonSet::const_iterator itr = dungeons->begin(); itr != dungeons->end(); ++itr)
                AddToQueue(guid, *itr);
            if (type == LFG_TYPE_RAID)
                break;
            for (GroupReference *itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* pGroupMember = itr->getSource();
                if (pGroupMember && pGroupMember->IsInWorld())
                {
                    if(sWorld.getConfig(CONFIG_BOOL_RESTRICTED_LFG_CHANNEL))
                        pGroupMember->JoinLFGChannel();
                    pGroupMember->GetSession()->SendLfgUpdateParty(LFG_UPDATETYPE_JOIN_PROPOSAL, type);
                    pGroupMember->GetLFGPlayerState()->SetState(LFG_STATE_LFG);
                }
            }
            groupState->SetStatus(LFG_STATUS_NOT_SAVED);
            StartRoleCheck(pGroup);
            break;
        }
        default:
            BASIC_LOG("LFGMgr::Join:Error: group %u in impossible state %u for join.", guid.GetCounter(), pGroup->GetLFGGroupState()->GetState());
            return;
    }
}

void LFGMgr::Leave(Player* pPlayer)
{
    if (!pPlayer)
        return;

    if (Group* pGroup = pPlayer->GetGroup())
    {
        if (pGroup->GetLeaderGuid() != pPlayer->GetObjectGuid())
        {
            // Check cheating - only leader can leave the queue
            DEBUG_LOG("LFGMgr::Leave:Error: Player %u is in LFG Group and want leave without Groupleader.", pPlayer->GetObjectGuid().GetCounter());
            return;
        }
        LeaveGroup(pGroup);
    }
    else
    {
        LeavePlayer(pPlayer);
    }
}

void LFGMgr::LeaveGroup(Group* pGroup)
{
    if (!pGroup)
        return;

    ObjectGuid guid = pGroup->GetObjectGuid();
    RemoveFromQueue(guid);
    if (pGroup->GetLFGGroupState()->GetState() == LFG_STATE_ROLECHECK)
    {
        pGroup->GetLFGGroupState()->SetRoleCheckState(LFG_ROLECHECK_ABORTED);
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            Player* pGroupMember = itr->getSource();
            if (pGroupMember && pGroupMember->IsInWorld())
            {
                pGroupMember->GetSession()->SendLfgRoleCheckUpdate();
            }
        }
    }
    else if (pGroup->GetLFGGroupState()->GetState() == LFG_STATE_PROPOSAL)
    {
        LFGProposal* pProposal = pGroup->GetLFGGroupState()->GetProposal();
        if (pProposal)
            pProposal->SetDeleted();
    }

    LFGType type = pGroup->GetLFGGroupState()->GetType();
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pGroupMember = itr->getSource();
        if (pGroupMember && pGroupMember->IsInWorld())
        {
            pGroupMember->GetSession()->SendLfgUpdateParty(LFG_UPDATETYPE_REMOVED_FROM_QUEUE, type);
            RemoveFromQueue(pGroupMember->GetObjectGuid());
            if(sWorld.getConfig(CONFIG_BOOL_RESTRICTED_LFG_CHANNEL) && pGroupMember->GetSession()->GetSecurity() == SEC_PLAYER )
                pGroupMember->LeaveLFGChannel();
            pGroupMember->GetLFGPlayerState()->Clear();
        }
    }
    pGroup->GetLFGGroupState()->Clear();
    DEBUG_LOG("LFGMgr::LeaveGroup:Success: Group %u leave LFG. Type: %u", pGroup->GetObjectGuid().GetCounter(), type);
}

void LFGMgr::LeavePlayer(Player* pPlayer)
{
    if (!pPlayer)
        return;

    ObjectGuid guid = pPlayer->GetObjectGuid();
    RemoveFromQueue(guid);

    LFGType type = pPlayer->GetLFGPlayerState()->GetType();
    pPlayer->GetSession()->SendLfgUpdatePlayer(LFG_UPDATETYPE_REMOVED_FROM_QUEUE, type);
    if(sWorld.getConfig(CONFIG_BOOL_RESTRICTED_LFG_CHANNEL) && pPlayer->GetSession()->GetSecurity() == SEC_PLAYER )
        pPlayer->LeaveLFGChannel();
    pPlayer->GetSession()->SendLfgUpdateSearch(false);
    pPlayer->GetLFGPlayerState()->Clear();
    DEBUG_LOG("LFGMgr::LeavePlayer:Success: Player %u leave LFG. Type: %u", pPlayer->GetObjectGuid().GetCounter(), type);
}

LFGQueueInfo* LFGMgr::GetQueueInfo(LFGDungeonEntry const* pDungeon)
{
    ReadGuard Guard(GetLock());

    LFGQueueInfoMap::iterator queue = m_queueInfoMap.find(pDungeon);
    if (queue == m_queueInfoMap.end())
        return NULL;
    else
        return &queue->second;
}

void LFGMgr::AddToQueue(ObjectGuid guid, LFGDungeonEntry const* pDungeon)
{
    // we need a guid (group or player)
    if (guid.IsEmpty())
        return;

    // we doesn't add something without a valid type
    if (pDungeon->type == LFG_TYPE_NONE)
        return;

    // at first we delete clean from queue
    RemoveFromQueue(guid);
    LFGQueueInfo* pqInfo = GetQueueInfo(pDungeon);
    if (!pqInfo)
    {
        // create a new List element
        WriteGuard Guard(GetLock());
        LFGQueueInfo newLFGQueueInfo = LFGQueueInfo();
        m_queueInfoMap.insert(std::make_pair(pDungeon, newLFGQueueInfo));
    }

    // Joining process
    // we must be save, that we add this info in Queue
    pqInfo = GetQueueInfo(pDungeon);
    MANGOS_ASSERT(pqInfo);

    if (guid.IsGroup())
    {
        pqInfo->groupGuids.insert(guid);
    }
    else
    {
        pqInfo->playerGuids.insert(guid);
    }
    DEBUG_LOG("LFGMgr::AddToQueue: %s %u joined, type %u",(guid.IsGroup() ? "group" : "player"), guid.GetCounter(), pDungeon->type);
}

void LFGMgr::RemoveFromQueue(ObjectGuid guid)
{
    WriteGuard Guard(GetLock());
    LFGQueueInfoMap::iterator itr = m_queueInfoMap.begin();
    while (itr != m_queueInfoMap.end())
    {
        LFGDungeonEntry const* dungeon = itr->first;
        LFGQueueInfo* pInfo = &itr->second;
        DEBUG_LOG("LFGMgr::RemoveFromQueue: %s %u removed, dungeonID %u",(guid.IsGroup() ? "group" : "player"), guid.GetCounter(), dungeon->ID);
        if (guid.IsGroup())
        {
            pInfo->groupGuids.erase(guid);
        }
        else
        {
            pInfo->playerGuids.erase(guid);
        }
        if (pInfo->playerGuids.empty() && pInfo->groupGuids.empty())
        {
            DEBUG_LOG("LFGMgr::RemoveFromQueue: dungeonID %u contains no members. Delete", dungeon->ID);
            m_queueInfoMap.erase(itr++);
        }
        else
            ++itr;
    }
}

LFGJoinResult LFGMgr::GetPlayerJoinResult(Player* pPlayer, LFGDungeonSet const* dungeons)
{

   if (pPlayer->InBattleGround() || pPlayer->InArena() || pPlayer->InBattleGroundQueue())
        return ERR_LFG_CANT_USE_DUNGEONS;

   if (pPlayer->HasAura(LFG_SPELL_DUNGEON_DESERTER))
        return  ERR_LFG_DESERTER_PLAYER;

    if (pPlayer->HasAura(LFG_SPELL_DUNGEON_COOLDOWN)
        && (!pPlayer->GetGroup() || pPlayer->GetGroup()->GetLFGGroupState()->GetStatus() != LFG_STATUS_OFFER_CONTINUE))
        return ERR_LFG_RANDOM_COOLDOWN_PLAYER;

    if (pPlayer->GetPlayerbotMgr() || pPlayer->GetPlayerbotAI())
    {
        DEBUG_LOG("LFGMgr::GetPlayerJoinResult: %u trying to join to dungeon finder, but has playerbots (or playerbot itself). Aborting.", pPlayer->GetObjectGuid().GetCounter());
        return ERR_LFG_NO_SLOTS_PLAYER;
    }

    // TODO - Check if all dungeons are valid

    // must be last check - ignored in party
    if (!dungeons || !dungeons->size())
        return ERR_LFG_INVALID_SLOT;

    return ERR_LFG_OK;
}

LFGJoinResult LFGMgr::GetGroupJoinResult(Group* group, LFGDungeonSet const* dungeons)
{
    if (!group)
        return ERR_LFG_GET_INFO_TIMEOUT;

    if (!group->isRaidGroup() && (group->GetMembersCount() > MAX_GROUP_SIZE))
        return ERR_LFG_TOO_MANY_MEMBERS;

    if (group->isRaidGroup() && group->GetLFGGroupState()->GetType() != LFG_TYPE_RAID)
    {
        DEBUG_LOG("LFGMgr::GetGroupJoinResult: Group %u trying to join as raid, but not to raid finder. Aborting.", group->GetObjectGuid().GetCounter());
        return ERR_LFG_MISMATCHED_SLOTS;
    }

    for (GroupReference *itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pGroupMember = itr->getSource();

        if (!pGroupMember->IsInWorld())
            return ERR_LFG_MEMBERS_NOT_PRESENT;

        LFGJoinResult result = GetPlayerJoinResult(pGroupMember, dungeons);

        if (result == ERR_LFG_INVALID_SLOT)
            continue;

        if (result != ERR_LFG_OK)
            return result;
    }

    return ERR_LFG_OK;
}

LFGLockStatusMap LFGMgr::GetPlayerLockMap(Player* pPlayer)
{
    LFGLockStatusMap tmpMap;
    tmpMap.clear();

    if (!pPlayer)
        return tmpMap;

    for (uint32 i = 1; i < sLFGDungeonStore.GetNumRows(); ++i)
    {
        if (LFGDungeonEntry const* entry = sLFGDungeonStore.LookupEntry(i))
            if (LFGLockStatusType status = GetPlayerLockStatus(pPlayer, entry))
                if (status != LFG_LOCKSTATUS_OK)
                    tmpMap.insert(std::make_pair(entry,status));
    }

    return tmpMap;
}

LFGLockStatusType LFGMgr::GetPlayerLockStatus(Player* pPlayer, LFGDungeonEntry const* dungeon)
{
    if (!pPlayer || !pPlayer->IsInWorld())
        return LFG_LOCKSTATUS_RAID_LOCKED;

    if (sWorld.IsDungeonMapIdDisable(dungeon->map))
        return LFG_LOCKSTATUS_NOT_FILL_REQUIREMENTS;

    // check if player in this dungeon. not need other checks
    if (pPlayer->GetGroup() && pPlayer->GetGroup()->isLFDGroup())
    {
        if (pPlayer->GetGroup()->GetLFGGroupState()->GetChosenDungeon())
        {
            if (pPlayer->GetGroup()->GetLFGGroupState()->GetChosenDungeon()->map == pPlayer->GetMapId())
                return LFG_LOCKSTATUS_OK;
        }
    }

    if (dungeon->expansion > pPlayer->GetSession()->Expansion())
        return LFG_LOCKSTATUS_INSUFFICIENT_EXPANSION;

    if (dungeon->difficulty > DUNGEON_DIFFICULTY_NORMAL
        && pPlayer->GetBoundInstance(dungeon->map, Difficulty(dungeon->difficulty)))
        return  LFG_LOCKSTATUS_RAID_LOCKED;

    if (dungeon->minlevel > pPlayer->getLevel())
        return  LFG_LOCKSTATUS_TOO_LOW_LEVEL;

    if (dungeon->maxlevel < pPlayer->getLevel())
        return LFG_LOCKSTATUS_TOO_HIGH_LEVEL;

    /* I think we doesn't need this check. If we use:
     * TODO: Check AREA_LOCKSTATUS_RAID_LOCKED. If Player is not in raid group he can't join this raid-Queue.
    switch (pPlayer->GetAreaLockStatus(dungeon->map, Difficulty(dungeon->difficulty)))
    {
        case AREA_LOCKSTATUS_OK:
            break;
        case AREA_LOCKSTATUS_TOO_LOW_LEVEL:
            return  LFG_LOCKSTATUS_TOO_LOW_LEVEL;
        case AREA_LOCKSTATUS_QUEST_NOT_COMPLETED:
            return LFG_LOCKSTATUS_QUEST_NOT_COMPLETED;
        case AREA_LOCKSTATUS_MISSING_ITEM:
            return LFG_LOCKSTATUS_MISSING_ITEM;
        case AREA_LOCKSTATUS_MISSING_DIFFICULTY:
            return LFG_LOCKSTATUS_RAID_LOCKED;
        case AREA_LOCKSTATUS_INSUFFICIENT_EXPANSION:
            return LFG_LOCKSTATUS_INSUFFICIENT_EXPANSION;
        case AREA_LOCKSTATUS_NOT_ALLOWED:
            return LFG_LOCKSTATUS_RAID_LOCKED;
        case AREA_LOCKSTATUS_RAID_LOCKED:
        case AREA_LOCKSTATUS_UNKNOWN_ERROR:
        default:
            return LFG_LOCKSTATUS_RAID_LOCKED;
    }*/

    if (dungeon->difficulty > DUNGEON_DIFFICULTY_NORMAL)
    {
        if (AreaTrigger const* at = sObjectMgr.GetMapEntranceTrigger(dungeon->map))
        {
            uint32 gs = pPlayer->GetEquipGearScore(true,true);

            if (at->minGS > 0 && gs < at->minGS)
                return LFG_LOCKSTATUS_TOO_LOW_GEAR_SCORE;
            else if (at->maxGS > 0 && gs > at->maxGS)
                return LFG_LOCKSTATUS_TOO_HIGH_GEAR_SCORE;
        }
        else
            return LFG_LOCKSTATUS_RAID_LOCKED;
    }

    if (InstancePlayerBind* bind = pPlayer->GetBoundInstance(dungeon->map, Difficulty(dungeon->difficulty)))
    {
        if (DungeonPersistentState* state = bind->state)
            if (state->IsCompleted())
                return LFG_LOCKSTATUS_RAID_LOCKED;
    }

        /* TODO
            LFG_LOCKSTATUS_ATTUNEMENT_TOO_LOW_LEVEL;
            LFG_LOCKSTATUS_ATTUNEMENT_TOO_HIGH_LEVEL;
            LFG_LOCKSTATUS_NOT_IN_SEASON;
        */

    return LFG_LOCKSTATUS_OK;
}

LFGLockStatusType LFGMgr::GetPlayerExpansionLockStatus(Player* pPlayer, LFGDungeonEntry const* dungeon)
{
    if (!pPlayer || !pPlayer->IsInWorld() || !dungeon)
        return LFG_LOCKSTATUS_RAID_LOCKED;

    uint32 randomEntry = 0;
    if (pPlayer->GetGroup() && pPlayer->GetGroup()->isLFDGroup())
    {
        if (pPlayer->GetGroup()->GetLFGGroupState()->GetChosenDungeon())
        {
            if (pPlayer->GetGroup()->GetLFGGroupState()->GetType() == LFG_TYPE_RANDOM_DUNGEON)
                randomEntry = (*pPlayer->GetGroup()->GetLFGGroupState()->GetDungeons()->begin())->ID;
        }
    }

    LFGDungeonExpansionEntry const* dungeonExpansion = NULL;

    for (uint32 i = 0; i < sLFGDungeonExpansionStore.GetNumRows(); ++i)
    {
        if (LFGDungeonExpansionEntry const* dungeonEx = sLFGDungeonExpansionStore.LookupEntry(i))
        {
            if (dungeonEx->dungeonID == dungeon->ID
                && dungeonEx->expansion == pPlayer->GetSession()->Expansion()
                && (randomEntry && randomEntry == dungeonEx->randomEntry))
                dungeonExpansion = dungeonEx;
        }
    }

    if (!dungeonExpansion)
        return LFG_LOCKSTATUS_OK;

    if (dungeonExpansion->minlevelHard > pPlayer->getLevel())
        return  LFG_LOCKSTATUS_TOO_LOW_LEVEL;

    if (dungeonExpansion->maxlevelHard < pPlayer->getLevel())
        return LFG_LOCKSTATUS_TOO_HIGH_LEVEL;

/*
    // need special case for handle attunement
    if (dungeonExpansion->minlevel > player->getLevel())
        return  LFG_LOCKSTATUS_ATTUNEMENT_TOO_LOW_LEVEL;

    if (dungeonExpansion->maxlevel < player->getLevel())
        return LFG_LOCKSTATUS_ATTUNEMENT_TOO_HIGH_LEVEL;
*/
        return LFG_LOCKSTATUS_OK;
}

LFGLockStatusType LFGMgr::GetGroupLockStatus(Group* group, LFGDungeonEntry const* dungeon)
{
    if (!group)
        return LFG_LOCKSTATUS_RAID_LOCKED;

    for (GroupReference *itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pGroupMember = itr->getSource();

        LFGLockStatusType result = GetPlayerLockStatus(pGroupMember, dungeon);

        if (result != LFG_LOCKSTATUS_OK)
            return result;
    }
    return LFG_LOCKSTATUS_OK;
}

LFGDungeonSet LFGMgr::GetRandomDungeonsForPlayer(Player* pPlayer)
{
    LFGDungeonSet list;

    for (uint32 i = 0; i < sLFGDungeonStore.GetNumRows(); ++i)
    {
        if (LFGDungeonEntry const* dungeon = sLFGDungeonStore.LookupEntry(i))
        {
            if (dungeon->type == LFG_TYPE_RANDOM_DUNGEON &&
                GetPlayerLockStatus(pPlayer, dungeon) == LFG_LOCKSTATUS_OK)
                list.insert(dungeon);
        }
    }
    return list;
}

LFGDungeonSet LFGMgr::ExpandRandomDungeonsForGroup(LFGDungeonEntry const* randomDungeon, GuidSet playerGuids)
{
    LFGDungeonSet list;
    for (uint32 i = 0; i < sLFGDungeonStore.GetNumRows(); ++i)
    {
        if (LFGDungeonEntry const* dungeonEx = sLFGDungeonStore.LookupEntry(i))
        {
            if (sWorld.IsDungeonMapIdDisable(dungeonEx->map))
                continue;

            if ((dungeonEx->type == LFG_TYPE_DUNGEON ||
                 dungeonEx->type == LFG_TYPE_HEROIC_DUNGEON)
                 && dungeonEx->difficulty == randomDungeon->difficulty)
            {
                bool checkPassed = true;
                for (GuidSet::const_iterator itr =  playerGuids.begin(); itr !=  playerGuids.end(); ++itr)
                {
                    Player* pPlayer = sObjectMgr.GetPlayer(*itr);

                    //TODO: Additional checks for expansion there!

                    if (!dungeonEx || GetPlayerLockStatus(pPlayer, dungeonEx) != LFG_LOCKSTATUS_OK)
                       checkPassed = false;
                }
                if (checkPassed)
                    list.insert(dungeonEx);
            }
        }
    }
    return list;
}

LFGDungeonEntry const* LFGMgr::GetDungeon(uint32 dungeonID)
{
    LFGDungeonMap::const_iterator itr = m_dungeonMap.find(dungeonID);
    return itr != m_dungeonMap.end() ? itr->second : NULL;
}

void LFGMgr::ClearLFRList(Player* pPlayer)
{
    if (!sWorld.getConfig(CONFIG_BOOL_LFG_ENABLE) && !sWorld.getConfig(CONFIG_BOOL_LFR_ENABLE))
        return;

    if (!pPlayer)
        return;

    LFGDungeonSet dungeons;
    dungeons.clear();
    pPlayer->GetLFGPlayerState()->SetDungeons(dungeons);
    DEBUG_LOG("LFGMgr::LFR List cleared, player %u leaving LFG queue", pPlayer->GetObjectGuid().GetCounter());
    RemoveFromQueue(pPlayer->GetObjectGuid());
}

GuidSet LFGMgr::GetDungeonPlayerQueue(LFGDungeonEntry const* pDungeon, Team team)
{
    GuidSet tmpSet;
    tmpSet.clear();
    if (!pDungeon)
        return tmpSet;

    ReadGuard Guard(GetLock());
    if (LFGQueueInfo* info = GetQueueInfo(pDungeon))
    {
        GuidSet players = info->playerGuids;
        for (GuidSet::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            ObjectGuid guid = (*itr);
            Player* pPlayer = sObjectMgr.GetPlayer(guid);
            if (!pPlayer)
                continue;

            if (team && pPlayer->GetTeam() != team)
                continue;

            if (pPlayer->GetLFGPlayerState()->GetState() < LFG_STATE_LFR ||
                pPlayer->GetLFGPlayerState()->GetState() > LFG_STATE_PROPOSAL)
                continue;

            tmpSet.insert(guid);
        }
    }
    return tmpSet;
}

GuidSet LFGMgr::GetDungeonGroupQueue(LFGDungeonEntry const* pDungeon, Team team)
{
    GuidSet tmpSet;
    tmpSet.clear();
    if (!pDungeon)
        return tmpSet;

    ReadGuard Guard(GetLock());
    if (LFGQueueInfo* pInfo = GetQueueInfo(pDungeon))
    {
        GuidSet groupguids = pInfo->groupGuids;
        for (GuidSet::const_iterator itr = groupguids.begin(); itr != groupguids.end(); ++itr)
        {
            Group* pGroup = sObjectMgr.GetGroup(*itr);
            if (!pGroup)
                continue;

            if (pGroup->GetLFGGroupState()->GetState() < LFG_STATE_LFR ||
                pGroup->GetLFGGroupState()->GetState() > LFG_STATE_PROPOSAL)
                continue;

            Player* pPlayer = sObjectMgr.GetPlayer(pGroup->GetLeaderGuid());
            if (!pPlayer)
                continue;

            if (team && pPlayer->GetTeam() != team)
                continue;

            tmpSet.insert(*itr);
        }
    }
    return tmpSet;
}

void LFGMgr::SendLFGRewards(Group* pGroup)
{
    if (!sWorld.getConfig(CONFIG_BOOL_LFG_ENABLE) && !sWorld.getConfig(CONFIG_BOOL_LFR_ENABLE))
        return;

    if (!pGroup || !pGroup->isLFGGroup())
    {
        DEBUG_LOG("LFGMgr::SendLFGReward: not group or not a LFGGroup. Ignoring");
        return;
    }

    if (pGroup->GetLFGGroupState()->GetState() == LFG_STATE_FINISHED_DUNGEON)
    {
        DEBUG_LOG("LFGMgr::SendLFGReward: group %u already rewarded!",pGroup->GetObjectGuid().GetCounter());
        return;
    }

    pGroup->GetLFGGroupState()->SetState(LFG_STATE_FINISHED_DUNGEON);
    pGroup->GetLFGGroupState()->SetStatus(LFG_STATUS_SAVED);


    LFGDungeonEntry const* pChosenDungeon = pGroup->GetLFGGroupState()->GetChosenDungeon();
    LFGDungeonEntry const* pDeclaredDungeon = *(pGroup->GetLFGGroupState()->GetDungeons()->begin());

    if (!pDeclaredDungeon)
    {
        DEBUG_LOG("LFGMgr::SendLFGReward: group %u - but no declared dungeon", pGroup->GetObjectGuid().GetCounter());
        return;
    }
    else if (!pChosenDungeon)
    {
        DEBUG_LOG("LFGMgr::SendLFGReward: group %u - but no chosen dungeon", pGroup->GetObjectGuid().GetCounter());
        return;
    }
    else  if (LFGType(pDeclaredDungeon->type) != LFG_TYPE_RANDOM_DUNGEON)
    {
        DEBUG_LOG("LFGMgr::SendLFGReward: group %u dungeon %u is not random (%u)", pGroup->GetObjectGuid().GetCounter(), pDeclaredDungeon->ID, pDeclaredDungeon->type);
        return;
    }

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pGroupMember = itr->getSource();
        if (pGroupMember && pGroupMember->IsInWorld())
            SendLFGReward(pGroupMember, pChosenDungeon, pDeclaredDungeon);
    }
}

void LFGMgr::SendLFGReward(Player* pPlayer, LFGDungeonEntry const* pChosenDungeon, LFGDungeonEntry const* pDeclaredDungeon)
{
    if (!pPlayer || !pDeclaredDungeon)
        return;

    if (pPlayer->GetLFGPlayerState()->GetState() == LFG_STATE_FINISHED_DUNGEON)
    {
        DEBUG_LOG("LFGMgr::SendLFGReward: player %u already rewarded!",pPlayer->GetObjectGuid().GetCounter());
        return;
    }

    // Update achievements
    if (pChosenDungeon->difficulty == DUNGEON_DIFFICULTY_HEROIC)
    {
        if (Group* pGroup = pPlayer->GetGroup())
        {
            if (pGroup->GetLFGGroupState()->GetRandomPlayersCount())
                pPlayer->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_USE_LFD_TO_GROUP_WITH_PLAYERS, pGroup->GetLFGGroupState()->GetRandomPlayersCount());
        }
    }

    pPlayer->GetLFGPlayerState()->SetState(LFG_STATE_FINISHED_DUNGEON);

    LFGReward const* reward = GetRandomDungeonReward(pDeclaredDungeon, pPlayer);

    if (!reward)
    {
        DEBUG_LOG("LFGMgr::SendLFGReward: We have no reward for player %u!",pPlayer->GetObjectGuid().GetCounter());
        return;
    }

    uint8 index = 0;

    // if we can take the quest, means that we haven't done this kind of "run", IE: First Heroic Random of Day.
    Quest const* pQuest = sObjectMgr.GetQuestTemplate(reward->reward[0].questId);

    if (!pPlayer->CanTakeQuest(pQuest, false))
        index = 1;

    Quest const* qReward = sObjectMgr.GetQuestTemplate(reward->reward[index].questId);

    if (!qReward)
    {
        sLog.outError("LFGMgr::SendLFGReward quest %u is absent in DB.", reward->reward[index].questId);
        return;
    }

    // we give reward without informing client (retail does this)
    pPlayer->RewardQuest(qReward,0,NULL,false);

    // Give rewards
    DEBUG_LOG("LFGMgr::SendLFGReward: %u done dungeon %u, %s previously done.", pPlayer->GetObjectGuid().GetCounter(), pChosenDungeon->ID, index > 0 ? " " : " not");
    pPlayer->GetSession()->SendLfgPlayerReward(pDeclaredDungeon, pChosenDungeon, reward, qReward, index != 0);
}

uint32 LFGMgr::CreateProposal(LFGDungeonEntry const* dungeon, Group* pGroup, GuidSet playerGuids)
{
    if (!dungeon)
        return false;

    uint32 ID = 0;
    if (pGroup)
    {
        if (LFGProposal* pProposal = pGroup->GetLFGGroupState()->GetProposal())
        {
            ID = pProposal->m_uiID;
        }
    }

    LFGProposal proposal = LFGProposal(dungeon);
    proposal.SetState(LFG_PROPOSAL_INITIATING);
    proposal.SetGroup(pGroup);
    if (ID)
    {
        WriteGuard Guard(GetLock());
        m_proposalMap.erase(ID);
        proposal.m_uiID = ID;
        m_proposalMap.insert(std::make_pair(ID, proposal));
    }
    else
    {
        WriteGuard Guard(GetLock());
        ID = GenerateProposalID();
        proposal.m_uiID = ID;
        m_proposalMap.insert(std::make_pair(ID, proposal));
    }

    LFGProposal* pProposal = GetProposal(ID);
    MANGOS_ASSERT(pProposal);
    pProposal->Start();

    if (pGroup)
    {
        pGroup->GetLFGGroupState()->SetProposal(pProposal);
        pGroup->GetLFGGroupState()->SetState(LFG_STATE_PROPOSAL);
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            Player* pGroupMember = itr->getSource();
            if (pGroupMember && pGroupMember->IsInWorld())
            {
                pGroupMember->GetSession()->SendLfgUpdateParty(LFG_UPDATETYPE_PROPOSAL_BEGIN, LFGType(dungeon->type));
                pGroupMember->GetSession()->SendLfgUpdateProposal(pProposal);
                pGroupMember->GetLFGPlayerState()->SetState(LFG_STATE_PROPOSAL);
                pGroupMember->GetLFGPlayerState()->SetAnswer(LFG_ANSWER_PENDING);
            }
        }
        if (!playerGuids.empty())
            pGroup->GetLFGGroupState()->SetRandomPlayersCount(playerGuids.size());
    }
    if (!playerGuids.empty())
    {
        for (GuidSet::const_iterator itr2 = playerGuids.begin(); itr2 != playerGuids.end(); ++itr2 )
        {
            if (!SendProposal(ID,*itr2))
                DEBUG_LOG("LFGMgr::CreateProposal: cannot send proposal %u, dungeon %u, %s to player %u", ID, dungeon->ID, pGroup ? " in pGroup" : " not in pGroup", (*itr2).GetCounter());
        }
    }
    DEBUG_LOG("LFGMgr::CreateProposal: %u, dungeon %u, %s", ID, dungeon->ID, pGroup ? " in pGroup" : " not in pGroup");
    return ID;
}

bool LFGMgr::SendProposal(uint32 ID, ObjectGuid guid)
{
    if (guid.IsEmpty() || !ID)
        return false;

    LFGProposal* pProposal = GetProposal(ID);

    Player* pPlayer = sObjectMgr.GetPlayer(guid);

    if (!pProposal || !pPlayer)
        return false;

    pProposal->AddMember(pPlayer);
    pProposal->Start();
    pPlayer->GetLFGPlayerState()->SetState(LFG_STATE_PROPOSAL);
    pPlayer->GetLFGPlayerState()->SetAnswer(LFG_ANSWER_PENDING);
    pPlayer->GetLFGPlayerState()->SetProposal(pProposal);
    if (pPlayer->GetGroup())
        pPlayer->GetSession()->SendLfgUpdateParty(LFG_UPDATETYPE_PROPOSAL_BEGIN, LFGType(pProposal->GetDungeon()->type));
    else
    {
        pPlayer->GetSession()->SendLfgUpdatePlayer(LFG_UPDATETYPE_GROUP_FOUND, LFGType(pProposal->GetDungeon()->type));
        pPlayer->GetSession()->SendLfgUpdatePlayer(LFG_UPDATETYPE_PROPOSAL_BEGIN, LFGType(pProposal->GetDungeon()->type));
    }

    pPlayer->GetSession()->SendLfgUpdateProposal(pProposal);

    if (pProposal->GetGroup())
    {
        pProposal->GetGroup()->GetLFGGroupState()->SetState(LFG_STATE_PROPOSAL);
    }

    DEBUG_LOG("LFGMgr::SendProposal: proposal %u, dungeon %u, %s", ID, pProposal->GetDungeon()->ID, pProposal->GetGroup() ? " in group" : " not in group");
    return true;
}

LFGProposal* LFGMgr::GetProposal(uint32 ID)
{
    LFGProposalMap::iterator itr = m_proposalMap.find(ID);
    return itr != m_proposalMap.end() ? &itr->second : NULL;
}


uint32 LFGMgr::GenerateProposalID()
{
    uint32 newID = m_proposalID;
    ++m_proposalID;
    return newID;
}

void LFGMgr::UpdateProposal(uint32 ID, ObjectGuid guid, bool accept)
{
    // Check if the proposal exists
    LFGProposal* pProposal = GetProposal(ID);
    if (!pProposal)
        return;

    Player* pPlayer = sObjectMgr.GetPlayer(guid);
    if (!pPlayer)
        return;

    // check player in proposal
    if (!pProposal->IsMember(pPlayer) && pProposal->GetGroup() && (pProposal->GetGroup() != pPlayer->GetGroup()) )
        return;

    pPlayer->GetLFGPlayerState()->SetAnswer(LFGAnswer(accept));

    // Remove member that didn't accept
    if (accept == LFG_ANSWER_DENY)
    {
        if (!sWorld.getConfig(CONFIG_BOOL_LFG_DEBUG_ENABLE))
            pPlayer->CastSpell(pPlayer,LFG_SPELL_DUNGEON_COOLDOWN,true);
        RemoveFomProposal(pPlayer, ID);
        return;
    }

    // check if all have answered and reorder players (leader first)
    bool allAnswered = true;
    GuidSet const proposalGuids = pProposal->GetMembers();
    for (GuidSet::const_iterator itr = proposalGuids.begin(); itr != proposalGuids.end(); ++itr )
    {
        Player* pPlayer = sObjectMgr.GetPlayer(*itr);
        if(pPlayer && pPlayer->IsInWorld())
        {
            if (pPlayer->GetLFGPlayerState()->GetAnswer() != LFG_ANSWER_AGREE)   // No answer (-1) or not accepted (0)
                allAnswered = false;
            pPlayer->GetSession()->SendLfgUpdateProposal(pProposal);
        }
    }

    if (Group* pGroup = pProposal->GetGroup())
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            Player* pGroupMember = itr->getSource();
            if(pGroupMember && pGroupMember->IsInWorld())
            {
                if (pGroupMember->GetLFGPlayerState()->GetAnswer() != LFG_ANSWER_AGREE)   // No answer (-1) or not accepted (0)
                    allAnswered = false;
                pGroupMember->GetSession()->SendLfgUpdateProposal(pProposal);
            }
        }
    }

    if (!allAnswered)
        return;

    DEBUG_LOG("LFGMgr::UpdateProposal: all players in proposal %u answered, make pGroup/teleport group", pProposal->m_uiID);
    // save waittime (group maked, save statistic)

    // Set the real dungeon (for random) or set old dungeon if OfferContinue

    LFGDungeonEntry const* chosenDungeon = NULL;
    MANGOS_ASSERT(pProposal->GetDungeon());

    // Create a new group (if needed)
    Group* pGroup = pProposal->GetGroup();

    if (pGroup && pGroup->GetLFGGroupState()->GetChosenDungeon())
        chosenDungeon = pGroup->GetLFGGroupState()->GetChosenDungeon();
    else
    {
        if (IsRandomDungeon(pProposal->GetDungeon()))
        {
            GuidSet tmpSet;
            if (pGroup)
            {
                for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
                {
                    Player* pGroupMember = itr->getSource();
                    if (pGroupMember && pGroupMember->IsInWorld())
                        tmpSet.insert(pGroupMember->GetObjectGuid());
                }
            }

            GuidSet const proposalGuids = pProposal->GetMembers();
            for (GuidSet::const_iterator itr = proposalGuids.begin(); itr != proposalGuids.end(); ++itr )
            {
                Player* pPlayer = sObjectMgr.GetPlayer(*itr);
                if (pPlayer && pPlayer->IsInWorld())
                    tmpSet.insert(pPlayer->GetObjectGuid());
            }

            LFGDungeonSet randomList = ExpandRandomDungeonsForGroup(pProposal->GetDungeon(), tmpSet);
            chosenDungeon = LFGMgr::SelectRandomDungeonFromList(&randomList);
            if (!chosenDungeon)
            {
                DEBUG_LOG("LFGMgr::UpdateProposal:%u cannot set real dungeon! no compatible list.", pProposal->m_uiID);
                pProposal->SetDeleted();
                return;
            }
        }
        else
            chosenDungeon = pProposal->GetDungeon();
    }

    if (!pGroup)
    {
        GuidSet proposalGuidsTmp = pProposal->GetMembers();
        if (proposalGuidsTmp.empty())
        {
            DEBUG_LOG("LFGMgr::UpdateProposal:%u cannot make pGroup, guid set is empty!", pProposal->m_uiID);
            pProposal->SetDeleted();
            return;
        }
        Player* pLeader = LeaderElection(&proposalGuidsTmp);
        if (!pLeader)
        {
            DEBUG_LOG("LFGMgr::UpdateProposal:%u cannot make pGroup, cannot set leader!", pProposal->m_uiID);
            pProposal->SetDeleted();
            return;
        }

        if (pLeader->GetGroup())
            pLeader->RemoveFromGroup();

        pLeader->GetSession()->SendLfgUpdatePlayer(LFG_UPDATETYPE_GROUP_FOUND, pLeader->GetLFGPlayerState()->GetType());
        pLeader->GetLFGPlayerState()->AddRole(ROLE_LEADER);

        pGroup = new Group();
        pGroup->Create(pLeader->GetObjectGuid(), pLeader->GetName());
        pGroup->ConvertToLFG(pProposal->GetType());
        sObjectMgr.AddGroup(pGroup);

        // LFG settings
        pGroup->GetLFGGroupState()->SetProposal(pProposal);
        pGroup->GetLFGGroupState()->SetState(LFG_STATE_PROPOSAL);
        pGroup->GetLFGGroupState()->AddDungeon(pProposal->GetDungeon());
        pGroup->GetLFGGroupState()->SetChosenDungeon(chosenDungeon);
        pGroup->GetLFGGroupState()->SetType(pLeader->GetLFGPlayerState()->GetType());

        // Special case to add leader to LFD pGroup:
        AddMemberToLFDGroup(pLeader->GetObjectGuid());
        pProposal->RemoveMember(pLeader);
        pLeader->GetLFGPlayerState()->SetProposal(NULL);
        DEBUG_LOG("LFGMgr::UpdateProposal: in proposal %u created pGroup %u", pProposal->m_uiID, pGroup->GetObjectGuid().GetCounter());
    }
    else
    {
        pGroup->GetLFGGroupState()->SetChosenDungeon(chosenDungeon);
        if (!pGroup->isLFGGroup())
        {
            pGroup->ConvertToLFG(pProposal->GetType());
            for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* pGroupMember = itr->getSource();
                if (pGroupMember && pGroupMember->IsInWorld())
                {
                    AddMemberToLFDGroup(pGroupMember->GetObjectGuid());
                    pGroupMember->GetSession()->SendLfgUpdateParty(LFG_UPDATETYPE_GROUP_FOUND, pGroup->GetLFGGroupState()->GetType());
                    if (!sWorld.getConfig(CONFIG_BOOL_LFG_DEBUG_ENABLE))
                        pGroupMember->CastSpell(pGroupMember,LFG_SPELL_DUNGEON_COOLDOWN,true);
                }
            }
        }
    }

    MANGOS_ASSERT(pGroup);
    pProposal->SetGroup(pGroup);
    pGroup->SendUpdate();

    // move players from proposal to pGroup
    for (GuidSet::const_iterator itr = proposalGuids.begin(); itr != proposalGuids.end(); ++itr)
    {
        Player* pPlayer = sObjectMgr.GetPlayer(*itr);
        if (pPlayer && pPlayer->IsInWorld() && pPlayer->GetMap())
        {
            if (pPlayer->GetGroup() && pPlayer->GetGroup() != pGroup)
            {
                pPlayer->RemoveFromGroup();
            }
            pPlayer->GetSession()->SendLfgUpdateParty(LFG_UPDATETYPE_GROUP_FOUND, pPlayer->GetLFGPlayerState()->GetType());
            // if already in Group, maybe with manual invite no double add or Groupleader at beginning in Group
            if (!pGroup->IsMember(pPlayer->GetObjectGuid()))
                pGroup->AddMember(pPlayer->GetObjectGuid(), pPlayer->GetName());

            pProposal->RemoveMember(pPlayer);
//            player->GetSession()->SendLfgUpdateProposal(pProposal);
            pPlayer->GetLFGPlayerState()->SetProposal(NULL);
            if (!sWorld.getConfig(CONFIG_BOOL_LFG_DEBUG_ENABLE))
                pPlayer->CastSpell(pPlayer,LFG_SPELL_DUNGEON_COOLDOWN,true);
        }
        else
        {
            pProposal->RemoveMember(pPlayer);
        }
    }

    // Update statistics for dungeon/roles/etc

    MANGOS_ASSERT(pGroup->GetLFGGroupState()->GetChosenDungeon());
    pGroup->SetDungeonDifficulty(Difficulty(pGroup->GetLFGGroupState()->GetChosenDungeon()->difficulty));
    pGroup->GetLFGGroupState()->SetStatus(LFG_STATUS_NOT_SAVED);
    pGroup->SendUpdate();

    // Teleport pGroup
    AddEvent(pGroup->GetObjectGuid(),LFG_EVENT_TELEPORT_GROUP);

    RemoveProposal(ID, true);
    pGroup->GetLFGGroupState()->SetState(LFG_STATE_IN_DUNGEON);
    RemoveFromQueue(pGroup->GetObjectGuid());
}

void LFGMgr::RemoveFomProposal(Player* pDecliner, uint32 ID)
{
    if (!pDecliner)
        return;

    LFGProposal* pProposal = GetProposal(ID);

    if (!pProposal || pProposal->IsDeleted())
        return;

    LFGType type = LFGType(pProposal->GetDungeon()->type);

    pProposal->SetDeleted();

    pDecliner->GetSession()->SendLfgUpdatePlayer(LFG_UPDATETYPE_PROPOSAL_DECLINED, type);

    if (pProposal->GetGroup() && pProposal->GetGroup() == pDecliner->GetGroup())
    {
        LeaveGroup(pDecliner->GetGroup());
    }
    else
    {
        pProposal->RemoveDecliner(pDecliner);
        LeavePlayer(pDecliner);
    }

    DEBUG_LOG("LFGMgr::UpdateProposal: %u didn't accept. Removing from queue", pDecliner->GetObjectGuid().GetCounter());

    if (Group* pGroup = pProposal->GetGroup())
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            Player* pGroupMember = itr->getSource();
            if (pGroupMember && pGroupMember->IsInWorld())
                pGroupMember->GetSession()->SendLfgUpdateParty(LFG_UPDATETYPE_PROPOSAL_DECLINED, type);
        }
    }

    GuidSet const playersSet = pProposal->GetMembers();
    if (!playersSet.empty())
    {
        for (GuidSet::const_iterator itr = playersSet.begin(); itr != playersSet.end(); ++itr)
        {
            ObjectGuid guid = *itr;

            if (guid.IsEmpty())
                continue;

            Player* pPlayer = sObjectMgr.GetPlayer(guid);
            if (pPlayer)
                pPlayer->GetSession()->SendLfgUpdatePlayer(LFG_UPDATETYPE_PROPOSAL_DECLINED, type);
        }
    }
}

void LFGMgr::RemoveProposal(uint32 ID, bool bGroupCreateSuccess)
{

    LFGProposal* pProposal = GetProposal(ID);

    if (!pProposal)
        return;

    pProposal->SetDeleted();

    if (!bGroupCreateSuccess)
    {
        GuidSet const proposalGuids = pProposal->GetMembers();
        for (GuidSet::const_iterator itr2 = proposalGuids.begin(); itr2 != proposalGuids.end(); ++itr2 )
        {
            Player* pPlayer = sObjectMgr.GetPlayer(*itr2);
            if (pPlayer)
            {
                pPlayer->GetSession()->SendLfgUpdatePlayer(LFG_UPDATETYPE_PROPOSAL_FAILED, LFGType(pProposal->GetDungeon()->type));
                pPlayer->GetLFGPlayerState()->SetProposal(NULL);

                // re-adding players to queue. decliner already removed
                if (pPlayer->GetLFGPlayerState()->GetAnswer() == LFG_ANSWER_AGREE)
                {
                    pPlayer->RemoveAurasDueToSpell(LFG_SPELL_DUNGEON_COOLDOWN);
                    AddToQueue(pPlayer->GetObjectGuid(),pProposal->GetDungeon());
                    pPlayer->GetLFGPlayerState()->SetState(LFG_STATE_QUEUED);
                    pPlayer->GetSession()->SendLfgJoinResult(ERR_LFG_OK, LFG_ROLECHECK_NONE);
                    pPlayer->GetSession()->SendLfgUpdatePlayer(LFG_UPDATETYPE_JOIN_PROPOSAL, LFGType(pProposal->GetDungeon()->type));
                    pPlayer->GetSession()->SendLfgUpdateSearch(true);
//                    player->GetSession()->SendLfgUpdatePlayer(LFG_UPDATETYPE_ADDED_TO_QUEUE, LFGType(pProposal->GetDungeon()->type));
                    DEBUG_LOG("LFGMgr::RemoveProposal: %u re-adding to queue", pPlayer->GetObjectGuid().GetCounter());
                }
                else
                    RemoveFromQueue(pPlayer->GetObjectGuid());
            }
        }

        if (Group* pGroup = pProposal->GetGroup())
        {
            pGroup->GetLFGGroupState()->SetProposal(NULL);
            pGroup->GetLFGGroupState()->SetState(LFG_STATE_QUEUED);
            AddToQueue(pGroup->GetObjectGuid(),pProposal->GetDungeon());

            for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* pGroupMember = itr->getSource();
                if (pGroupMember && pGroupMember->IsInWorld())
                {
                    pGroupMember->GetSession()->SendLfgUpdateParty(LFG_UPDATETYPE_PROPOSAL_FAILED, LFGType(pProposal->GetDungeon()->type));
                    pGroupMember->GetSession()->SendLfgUpdateParty(LFG_UPDATETYPE_ADDED_TO_QUEUE, LFGType(pProposal->GetDungeon()->type));
                    pGroupMember->GetSession()->SendLfgUpdateSearch(true);
                }
            }
        }
    }

    WriteGuard Guard(GetLock());
    m_proposalMap.erase(ID);
}

void LFGMgr::CleanupProposals()
{
    std::set<uint32> expiredProposals;
    for (LFGProposalMap::iterator itr = m_proposalMap.begin(); itr != m_proposalMap.end(); ++itr)
    {
        if (itr->second.IsExpired())
            expiredProposals.insert(itr->second.m_uiID);
        else if (itr->second.IsDeleted())
            expiredProposals.insert(itr->second.m_uiID);
    }

    if (!expiredProposals.empty())
    {
        for(std::set<uint32>::const_iterator itr = expiredProposals.begin(); itr != expiredProposals.end(); ++itr)
        {
            DEBUG_LOG("LFGMgr::CleanupProposals: remove expired proposal %u", *itr);
            RemoveProposal(*itr, false);
        }
    }
}

void LFGMgr::OfferContinue(Group* pGroup)
{
    if (!sWorld.getConfig(CONFIG_BOOL_LFG_ENABLE))
        return;

    if (pGroup)
    {
        LFGDungeonEntry const* dungeon = pGroup->GetLFGGroupState()->GetChosenDungeon();
        if (!dungeon ||  pGroup->GetLFGGroupState()->GetStatus() == LFG_STATUS_OFFER_CONTINUE || pGroup->GetLFGGroupState()->GetStatus() == LFG_STATUS_COMPLETE)
        {
            DEBUG_LOG("LFGMgr::OfferContinue: group %u not have required attributes! Status %u ", pGroup->GetObjectGuid().GetCounter(), pGroup->GetLFGGroupState()->GetStatus());
            return;
        }
        Player* pLeader = sObjectMgr.GetPlayer(pGroup->GetLeaderGuid());
        if (pLeader)
            pLeader->GetSession()->SendLfgOfferContinue(dungeon);
        pGroup->GetLFGGroupState()->SetStatus(LFG_STATUS_OFFER_CONTINUE);
    }
    else
        sLog.outError("LFGMgr::OfferContinue: no group!");
}

void LFGMgr::InitBoot(Player* pKicker, ObjectGuid victimGuid, std::string reason)
{
    Group*  pGroup = pKicker->GetGroup();
    Player* pVictim = sObjectMgr.GetPlayer(victimGuid);

    if (!pKicker || !pGroup || !pVictim)
        return;

    DEBUG_LOG("LFGMgr::InitBoot: group %u kicker %u victim %u reason %s", pGroup->GetObjectGuid().GetCounter(), pKicker->GetObjectGuid().GetCounter(), victimGuid.GetCounter(), reason.c_str());

    if (!pGroup->GetLFGGroupState()->IsBootActive())
    {
        pGroup->GetLFGGroupState()->SetVotesNeeded(ceil(float(pGroup->GetMembersCount())/2.0));
        pGroup->GetLFGGroupState()->StartBoot(pKicker->GetObjectGuid(), victimGuid, reason);
        m_GroupsWithBoot.insert(pGroup->GetObjectGuid());
    }
    else
    {
    // send error to player
    //    return;
    }

    if (pGroup->GetLFGGroupState()->GetKicksLeft() == 0)
    {
        pGroup->Disband();
    }

    // Notify players
    for (GroupReference *itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pGroupMember = itr->getSource();

        if (pGroupMember && pGroupMember->IsInWorld())
            pGroupMember->GetSession()->SendLfgBootPlayer();
    }
}

void LFGMgr::CleanupBoots()
{
    GuidSet expiredBoots;
    for (GuidSet::const_iterator groupWithBootitr = m_GroupsWithBoot.begin(); groupWithBootitr != m_GroupsWithBoot.end(); ++groupWithBootitr)
    {
        ObjectGuid groupGuid = *groupWithBootitr;

        Group* pGroup = sObjectMgr.GetGroup(groupGuid);
        if (!pGroup)
        {
            expiredBoots.insert(groupGuid);
            continue;
        }

        if (pGroup->GetLFGGroupState()->GetState() != LFG_STATE_BOOT)
        {
            expiredBoots.insert(groupGuid);
            continue;
        }

        if (pGroup->GetLFGGroupState()->IsBootActive())
            continue;

        for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            Player* pGroupMember = itr->getSource();
            if (pGroupMember && pGroupMember->IsInWorld())
            {
                pGroupMember->GetSession()->SendLfgBootPlayer();
            }
        }
        pGroup->GetLFGGroupState()->StopBoot();
        expiredBoots.insert(groupGuid);
    }

    if (!expiredBoots.empty())
    {
        for(GuidSet::const_iterator itr = expiredBoots.begin(); itr != expiredBoots.end(); ++itr)
        {
            DEBUG_LOG("LFGMgr::CleanupBoots: remove expired Boot for Group %u", (*itr).GetCounter());
            m_GroupsWithBoot.erase(*itr);
        }
    }
}

void LFGMgr::UpdateBoot(Player* pPlayer, LFGAnswer answer)
{
    Group* pGroup = pPlayer->GetGroup();
    if (!pGroup)
        return;

    if (!pGroup->GetLFGGroupState()->IsBootActive())
        return;

    DEBUG_LOG("LFGMgr::UpdateBoot: group %u kicker %u answer %u", pGroup->GetObjectGuid().GetCounter(), pPlayer->GetObjectGuid().GetCounter(), answer);

    pGroup->GetLFGGroupState()->UpdateBoot(pPlayer->GetObjectGuid(),answer);

    // Send update info to all players
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pGroupMember = itr->getSource();
        if (pGroupMember && pGroupMember->IsInWorld())
        {
            pGroupMember->GetSession()->SendLfgBootPlayer();
        }
    }

    switch (pGroup->GetLFGGroupState()->GetBootResult())
    {
        case LFG_ANSWER_AGREE:
        {
            Player* pVictim = sObjectMgr.GetPlayer(pGroup->GetLFGGroupState()->GetBootVictim());
            if (!pVictim)
            {
                pGroup->GetLFGGroupState()->StopBoot();
                return;
            }
            Player::RemoveFromGroup(pGroup, pVictim->GetObjectGuid());
            pVictim->GetLFGPlayerState()->Clear();

            // group may be disbanded after Player::RemoveFromGroup!
            pGroup = pPlayer->GetGroup();
            if (!pGroup)
                return;
            if (!pGroup->GetLFGGroupState()->IsBootActive())
                return;

            pGroup->GetLFGGroupState()->DecreaseKicksLeft();
            pGroup->GetLFGGroupState()->StopBoot();
            // CleanupBoots make the erase therefor "m_GroupsWithBoot.erase(pGroup->GetObjectGuid());" not necessary
            OfferContinue(pGroup);
            break;
        }
        case LFG_ANSWER_DENY:
            pGroup->GetLFGGroupState()->StopBoot();
            // CleanupBoots make the erase therefor "m_GroupsWithBoot.erase(pGroup->GetObjectGuid());" not necessary
            break;
        case LFG_ANSWER_PENDING:
            break;
        default:
            break;
    }
}

void LFGMgr::Teleport(Group* pGroup, bool out)
{
    if (!pGroup)
        return;

    DEBUG_LOG("LFGMgr::Teleport: %u %s dungeon!", pGroup->GetObjectGuid().GetCounter(), out ? "out" : "in");

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pGroupMember = itr->getSource();
        if (pGroupMember && pGroupMember->IsInWorld())
        {
            if (!pGroupMember->GetLFGPlayerState()->IsTeleported() && !out)
                AddEvent(pGroupMember->GetObjectGuid(),LFG_EVENT_TELEPORT_PLAYER, LONG_LFG_DELAY, uint8(out));
            else if (out)
                Teleport(pGroupMember, out);
        }
    }
    if (pGroup->GetLFGGroupState()->GetState() == LFG_STATE_LFG
        || pGroup->GetLFGGroupState()->GetState() == LFG_STATE_LFR)
        pGroup->GetLFGGroupState()->SetState(LFG_STATE_IN_DUNGEON);

    pGroup->SendUpdate();
}

void LFGMgr::Teleport(Player* pPlayer, bool out, bool fromOpcode /*= false*/)
{
    if (!pPlayer || pPlayer->IsInCombat())
        return;

    DEBUG_LOG("LFGMgr::TeleportPlayer: %u is being teleported %s", pPlayer->GetObjectGuid().GetCounter(), out ? "from dungeon." : "in dungeon.");

    if (out)
    {
        pPlayer->RemoveAurasDueToSpell(LFG_SPELL_LUCK_OF_THE_DRAW);
        pPlayer->TeleportToBGEntryPoint();
        return;
    }

    // TODO Add support for LFG_TELEPORTERROR_FATIGUE
    LFGTeleportError error = LFG_TELEPORTERROR_OK;

    Group* pGroup = pPlayer->GetGroup();

    if (!pGroup)
        error = LFG_TELEPORTERROR_UNK4;
    else if (!pPlayer->isAlive())
        error = LFG_TELEPORTERROR_PLAYER_DEAD;
    else if (pPlayer->IsFalling())
        error = LFG_TELEPORTERROR_FALLING;

    uint32 mapid = 0;
    float x = 0;
    float y = 0;
    float z = 0;
    float orientation = 0;
    Difficulty difficulty;

    LFGDungeonEntry const* dungeon = NULL;

    if (error == LFG_TELEPORTERROR_OK)
    {
        dungeon = pGroup->GetLFGGroupState()->GetChosenDungeon();
        if (!dungeon)
        {
            error = LFG_TELEPORTERROR_INVALID_LOCATION;
            DEBUG_LOG("LFGMgr::TeleportPlayer %u error %u, no dungeon!", pPlayer->GetObjectGuid().GetCounter(), error);
        }
    }

    if (error == LFG_TELEPORTERROR_OK)
    {
        difficulty = Difficulty(dungeon->difficulty);
        bool leaderInDungeon = false;
        Player* leader = sObjectMgr.GetPlayer(pGroup->GetLeaderGuid());
        if (leader && pPlayer != leader && leader->GetMapId() == uint32(dungeon->map))
            leaderInDungeon = true;

        if (pGroup->GetDungeonDifficulty() != Difficulty(dungeon->difficulty))
        {
            error = LFG_TELEPORTERROR_UNK4;
            DEBUG_LOG("LFGMgr::TeleportPlayer %u error %u, difficulty not match!", pPlayer->GetObjectGuid().GetCounter(), error);
        }
        else if (GetPlayerLockStatus(pPlayer,dungeon) != LFG_LOCKSTATUS_OK)
        {
            error = LFG_TELEPORTERROR_INVALID_LOCATION;
            DEBUG_LOG("LFGMgr::TeleportPlayer %u error %u, player not enter to this instance!", pPlayer->GetObjectGuid().GetCounter(), error);
        }
        else if (leaderInDungeon && pGroup->GetLFGGroupState()->GetState() == LFG_STATE_IN_DUNGEON)
        {
            mapid = leader->GetMapId();
            x = leader->GetPositionX();
            y = leader->GetPositionY();
            z = leader->GetPositionZ();
            orientation = leader->GetOrientation();
        }
        else if (AreaTrigger const* at = sObjectMgr.GetMapEntranceTrigger(dungeon->map))
        {
            mapid = at->target_mapId;
            x = at->target_X;
            y = at->target_Y;
            z = at->target_Z;
            orientation = at->target_Orientation;
        }
        else
        {
            error = LFG_TELEPORTERROR_INVALID_LOCATION;
            DEBUG_LOG("LFGMgr::TeleportPlayer %u error %u, no areatrigger to map %u!", pPlayer->GetObjectGuid().GetCounter(), error, dungeon->map);
        }
    }

    if (error == LFG_TELEPORTERROR_OK)
    {

        if (pPlayer->GetMap() && !pPlayer->GetMap()->IsDungeon() && !pPlayer->GetMap()->IsRaid() && !pPlayer->InBattleGround())
            pPlayer->SetBattleGroundEntryPoint(true);

        // stop taxi flight at port
        if (pPlayer->IsTaxiFlying())
            pPlayer->InterruptTaxiFlying();

        pPlayer->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);
        pPlayer->RemoveSpellsCausingAura(SPELL_AURA_FLY);

        DETAIL_LOG("LFGMgr: Sending %s to map %u, difficulty %u X %f, Y %f, Z %f, O %f", pPlayer->GetName(), uint8(difficulty), mapid, x, y, z, orientation);

        pPlayer->TeleportTo(mapid, x, y, z, orientation);
        pPlayer->GetLFGPlayerState()->SetState(LFG_STATE_IN_DUNGEON);
        pPlayer->GetLFGPlayerState()->SetTeleported();
    }
    else
        pPlayer->GetSession()->SendLfgTeleportError(error);
}

void LFGMgr::CleanupRoleChecks()
{
    for (LFGQueueInfoMap::const_iterator infoMapitr = m_queueInfoMap.begin(); infoMapitr != m_queueInfoMap.end(); ++infoMapitr)
    {
        // LFGDungeonEntry const* dungeon = infoMapitr->first;
        LFGQueueInfo const* info = &infoMapitr->second;
        GuidSet groupGuids = info->groupGuids;

        if (groupGuids.empty())
            continue;
        for (GuidSet::const_iterator itr = groupGuids.begin(); itr != groupGuids.end(); ++itr)
        {
            ObjectGuid groupGuid = *itr;

            Group* pGroup = sObjectMgr.GetGroup(groupGuid);
            if (!pGroup)
                continue;

            if (pGroup->GetLFGGroupState()->GetState() != LFG_STATE_ROLECHECK)
                continue;

            if (pGroup->GetLFGGroupState()->GetRoleCheckState() == LFG_ROLECHECK_FINISHED)
                continue;

            if (pGroup->GetLFGGroupState()->QueryRoleCheckTime())
                continue;

            pGroup->GetLFGGroupState()->SetRoleCheckState(LFG_ROLECHECK_MISSING_ROLE);

            for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* pGroupMember = itr->getSource();
                if (pGroupMember && pGroupMember->IsInWorld())
                {
                    pGroupMember->GetSession()->SendLfgRoleCheckUpdate();
                    pGroupMember->GetSession()->SendLfgJoinResult(ERR_LFG_ROLE_CHECK_FAILED, LFG_ROLECHECK_MISSING_ROLE);
                }
            }
            pGroup->GetLFGGroupState()->Clear();
            pGroup->GetLFGGroupState()->SetRoleCheckState(LFG_ROLECHECK_NONE);
            RemoveFromQueue(pGroup->GetObjectGuid());
        }
    }
}

void LFGMgr::StartRoleCheck(Group* pGroup)
{
    if (!pGroup)
        return;

    pGroup->GetLFGGroupState()->StartRoleCheck();
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pGroupMember = itr->getSource();
        if (pGroupMember && pGroupMember->IsInWorld())
        {
            if (pGroupMember->GetObjectGuid() != pGroup->GetLeaderGuid())
            {
                pGroupMember->GetLFGPlayerState()->SetRoles(LFG_ROLE_MASK_NONE);
                pGroupMember->GetLFGPlayerState()->SetState(LFG_STATE_ROLECHECK);
            }
            else
            {
                pGroupMember->GetLFGPlayerState()->AddRole(ROLE_LEADER);
                pGroupMember->GetLFGPlayerState()->SetState(LFG_STATE_QUEUED);
            }
            pGroupMember->GetSession()->SendLfgRoleCheckUpdate();
        }
    }
}

void LFGMgr::UpdateRoleCheck(Group* pGroup)
{
    if (!pGroup)
        return;

    if (pGroup->GetLFGGroupState()->GetState() != LFG_STATE_ROLECHECK)
        return;

    bool isFinished = true;
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pGroupMember = itr->getSource();
        if (pGroupMember && pGroupMember->IsInWorld())
        {
            if (pGroupMember->GetLFGPlayerState()->GetState() == LFG_STATE_QUEUED)
                continue;

            LFGRoleCheckState newstate = LFG_ROLECHECK_NONE;

            if (pGroupMember->GetLFGPlayerState()->GetRoles() < LFG_ROLE_MASK_TANK)
                newstate = LFG_ROLECHECK_INITIALITING;
            else
            {
                newstate = LFG_ROLECHECK_FINISHED;
                pGroupMember->GetLFGPlayerState()->SetState(LFG_STATE_QUEUED);
            }

            if (newstate != LFG_ROLECHECK_FINISHED)
                isFinished = false;
        }
    }


    if (!isFinished)
        pGroup->GetLFGGroupState()->SetRoleCheckState(LFG_ROLECHECK_INITIALITING);
    else if (!CheckRoles(pGroup))
        pGroup->GetLFGGroupState()->SetRoleCheckState(LFG_ROLECHECK_WRONG_ROLES);
    else
        pGroup->GetLFGGroupState()->SetRoleCheckState(LFG_ROLECHECK_FINISHED);

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pGroupMember = itr->getSource();
        if (pGroupMember && pGroupMember->IsInWorld())
        {
            pGroupMember->GetSession()->SendLfgRoleCheckUpdate();
        }
    }

    DEBUG_LOG("LFGMgr::UpdateRoleCheck group %u %s result %u", pGroup->GetObjectGuid().GetCounter(),isFinished ? "completed" : "not finished", pGroup->GetLFGGroupState()->GetRoleCheckState());

    // temporary - only all answer accept
    if (pGroup->GetLFGGroupState()->GetRoleCheckState() != LFG_ROLECHECK_FINISHED)
        return;

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pGroupMember = itr->getSource();
        if (pGroupMember && pGroupMember->IsInWorld())
        {
            pGroupMember->GetSession()->SendLfgRoleCheckUpdate();

            if (pGroup->GetLFGGroupState()->GetRoleCheckState() == LFG_ROLECHECK_FINISHED)
            {
                pGroupMember->GetLFGPlayerState()->SetJoined();
                pGroupMember->GetLFGPlayerState()->SetState(LFG_STATE_QUEUED);

                if (pGroupMember->GetObjectGuid() == pGroup->GetLeaderGuid())
                    pGroupMember->GetSession()->SendLfgUpdateParty(LFG_UPDATETYPE_ADDED_TO_QUEUE, pGroup->GetLFGGroupState()->GetType());
                else
                {
//                    member->GetSession()->SendLfgJoinResult(ERR_LFG_OK, LFG_ROLECHECK_NONE, true);
                    pGroupMember->GetSession()->SendLfgUpdatePlayer(LFG_UPDATETYPE_JOIN_PROPOSAL, pGroup->GetLFGGroupState()->GetType());
                }
            }
            else
            {
                pGroupMember->GetLFGPlayerState()->SetState(LFG_STATE_NONE);
                pGroupMember->GetSession()->SendLfgJoinResult(ERR_LFG_ROLE_CHECK_FAILED, LFG_ROLECHECK_MISSING_ROLE);
                pGroupMember->GetSession()->SendLfgUpdateParty(LFG_UPDATETYPE_ROLECHECK_FAILED, pGroup->GetLFGGroupState()->GetType());
            }
        }
    }

    DEBUG_LOG("LFGMgr::UpdateRoleCheck finished, group %u result %u", pGroup->GetObjectGuid().GetCounter(), pGroup->GetLFGGroupState()->GetRoleCheckState());

    if (pGroup->GetLFGGroupState()->GetRoleCheckState() == LFG_ROLECHECK_FINISHED)
    {
        pGroup->GetLFGGroupState()->SetState(LFG_STATE_QUEUED);
    }
    else
    {
        RemoveFromQueue(pGroup->GetObjectGuid());
    }
}

bool LFGMgr::CheckRoles(Group* pGroup, Player* pPlayer /*=NULL*/)
{
    if (!pGroup)
        return false;

    if (pGroup->isRaidGroup())
        return true;


    LFGRolesMap rolesMap;

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pGroupMember = itr->getSource();
        if (pGroupMember && pGroupMember->IsInWorld())
            rolesMap.insert(std::make_pair(pGroupMember->GetObjectGuid(), pGroupMember->GetLFGPlayerState()->GetRoles()));
    }

    if (pPlayer && pPlayer->IsInWorld())
        rolesMap.insert(std::make_pair(pPlayer->GetObjectGuid(), pPlayer->GetLFGPlayerState()->GetRoles()));

    return CheckRoles(&rolesMap);
}

bool LFGMgr::CheckRoles(LFGRolesMap const* rolesMap)
{
    if (!rolesMap || rolesMap->empty())
        return false;

    if (rolesMap->size() > MAX_GROUP_SIZE)
        return false;

    uint8 tanks   = LFG_TANKS_NEEDED;
    uint8 healers = LFG_HEALERS_NEEDED;
    uint8 dps     = LFG_DPS_NEEDED;
    std::vector<LFGRoleMask> rolesVector;

    for (LFGRolesMap::const_iterator itr = rolesMap->begin(); itr != rolesMap->end(); ++itr)
        rolesVector.push_back(LFGRoleMask(itr->second & ~LFG_ROLE_MASK_LEADER));

    std::sort(rolesVector.begin(),rolesVector.end());

    for (std::vector<LFGRoleMask>::const_iterator itr = rolesVector.begin(); itr != rolesVector.end(); ++itr)
    {
        if ((*itr & LFG_ROLE_MASK_TANK) && tanks > 0)
            --tanks;
        else if ((*itr & LFG_ROLE_MASK_HEALER) && healers > 0)
            --healers;
        else if ((*itr & LFG_ROLE_MASK_DAMAGE) && dps > 0)
            --dps;
    }

    DEBUG_LOG("LFGMgr::CheckRoles healers %u tanks %u dps %u map size " SIZEFMTD, healers, tanks, dps, rolesMap->size());

//    if (sWorld.getConfig(CONFIG_BOOL_LFG_DEBUG_ENABLE))
//        return true;

    if ((healers + tanks + dps) > int8(MAX_GROUP_SIZE - rolesMap->size()))
        return false;

    return true;
}

bool LFGMgr::RoleChanged(Player* pPlayer, LFGRoleMask roles)
{
    LFGRoleMask oldRoles = pPlayer->GetLFGPlayerState()->GetRoles();
    pPlayer->GetLFGPlayerState()->SetRoles(roles);

    // send Information about Role Change to Group
    if (Group* pGroup = pPlayer->GetGroup())
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            Player* pGroupMember = itr->getSource();
            if (pGroupMember && pGroupMember->IsInWorld())
                pGroupMember->GetSession()->SendLfgRoleChosen(pPlayer->GetObjectGuid(), roles);
        }
    }
    else
        pPlayer->GetSession()->SendLfgRoleChosen(pPlayer->GetObjectGuid(), roles);

    if (oldRoles != pPlayer->GetLFGPlayerState()->GetRoles())
        return true;

    return false;
}

Player* LFGMgr::LeaderElection(GuidSet* playerGuids)
{
    std::set<Player*> leaders;
    Player* pLeader = NULL;
    uint32 GS = 0;

    for (GuidSet::const_iterator itr = playerGuids->begin(); itr != playerGuids->end(); ++itr)
    {
        Player* pMember  = sObjectMgr.GetPlayer(*itr);
        if (pMember && pMember->IsInWorld())
        {
            if (pMember->GetLFGPlayerState()->HasRole(ROLE_LEADER))
                leaders.insert(pMember);

            pMember->GetLFGPlayerState()->RemoveRole(ROLE_LEADER);

            if (pMember->GetEquipGearScore() > GS)
            {
                GS = pMember->GetEquipGearScore();
                pLeader = pMember;
            }
        }
    }

    GS = 0;
    if (!leaders.empty())
    {
        for (std::set<Player*>::const_iterator itr = leaders.begin(); itr != leaders.end(); ++itr)
        {
            if ((*itr)->GetEquipGearScore() > GS)
            {
                GS = (*itr)->GetEquipGearScore();
                pLeader = (*itr);
            }
        }
    }

    if (!pLeader)
    {
        for (GuidSet::const_iterator itr = playerGuids->begin(); itr != playerGuids->end(); ++itr)
        {
            Player* pMember  = sObjectMgr.GetPlayer(*itr);
            if (pMember && pMember->IsInWorld())
            {
                pLeader = pMember;
                break;
            }
        }
    }
    // leader may be NULL!
    return pLeader;
}

void LFGMgr::SetRoles(LFGRolesMap* rolesMap)
{
    if (!rolesMap || rolesMap->empty())
        return;
    DEBUG_LOG("LFGMgr::SetRoles set roles for rolesmap size = %u",uint8(rolesMap->size()));

    LFGRoleMask oldRoles;
    LFGRoleMask newRole;
    ObjectGuid  tankGuid;
    ObjectGuid  healGuid;

    LFGRolesMap tmpMap;

    // strip double/triple roles
    for (LFGRolesMap::const_iterator itr = rolesMap->begin(); itr != rolesMap->end(); ++itr)
    {
        if (itr->second & LFG_ROLE_MASK_TANK)
            tmpMap.insert(*itr);
    }

    if (tmpMap.size() == 1)
    {
        tankGuid = tmpMap.begin()->first;
        newRole    = LFGRoleMask(tmpMap.begin()->second & ~LFG_ROLE_MASK_HEALER_DAMAGE);
    }
    else
    {
        for (LFGRolesMap::iterator itr = tmpMap.begin(); itr != tmpMap.end(); ++itr)
        {
            tankGuid = itr->first;
            LFGRolesMap::iterator itr2 = rolesMap->find(tankGuid);
            oldRoles = itr2->second;
            newRole    = LFGRoleMask(itr->second & ~LFG_ROLE_MASK_HEALER_DAMAGE);

            itr2->second = LFGRoleMask(newRole);

            if (CheckRoles(rolesMap))
                break;
            else
                itr2->second = oldRoles;
        }
    }
    rolesMap->find(tankGuid)->second = newRole;
    tmpMap.clear();

    for (LFGRolesMap::iterator itr = rolesMap->begin(); itr != rolesMap->end(); ++itr)
    {
        if (itr->second & LFG_ROLE_MASK_HEALER)
            tmpMap.insert(*itr);
    }

    if (tmpMap.size() == 1)
    {
        healGuid = tmpMap.begin()->first;
        newRole    = LFGRoleMask(tmpMap.begin()->second & ~LFG_ROLE_MASK_TANK_DAMAGE);
    }
    else
    {
        for (LFGRolesMap::iterator itr = tmpMap.begin(); itr != tmpMap.end(); ++itr)
        {
            healGuid = itr->first;
            LFGRolesMap::iterator itr2 = rolesMap->find(healGuid);
            oldRoles = itr2->second;
            newRole    = LFGRoleMask(itr->second & ~LFG_ROLE_MASK_TANK_DAMAGE);

            itr2->second = LFGRoleMask(newRole);

            if (CheckRoles(rolesMap))
                break;
            else
                itr2->second = oldRoles;
        }
    }
    rolesMap->find(healGuid)->second = newRole;
    tmpMap.clear();

    for (LFGRolesMap::iterator itr = rolesMap->begin(); itr != rolesMap->end(); ++itr)
    {
        if (itr->first != tankGuid && itr->first != healGuid)
        {
            newRole      = LFGRoleMask(itr->second & ~LFG_ROLE_MASK_TANK_HEALER);
            itr->second  = LFGRoleMask(newRole);
        }
    }

    for (LFGRolesMap::iterator itr = rolesMap->begin(); itr != rolesMap->end(); ++itr)
    {
        Player* pPlayer = sObjectMgr.GetPlayer(itr->first);
        if (pPlayer && pPlayer->IsInWorld())
        {
            pPlayer->GetLFGPlayerState()->SetRoles(itr->second);
            DEBUG_LOG("LFGMgr::SetRoles role for player %u set to %u",pPlayer->GetObjectGuid().GetCounter(), uint8(itr->second));
        }
    }

}

void LFGMgr::SetGroupRoles(Group* pGroup, GuidSet const* players)
{
    if (!pGroup)
        return;

    LFGRolesMap rolesMap;
    bool hasMultiRoles = false;

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pGroupMember = itr->getSource();
        if (pGroupMember && pGroupMember->IsInWorld())
        {
            rolesMap.insert(std::make_pair(pGroupMember->GetObjectGuid(), pGroupMember->GetLFGPlayerState()->GetRoles()));
            if (!pGroupMember->GetLFGPlayerState()->HasSingleRole())
                hasMultiRoles = true;
        }
    }


    for (GuidSet::const_iterator itr = players->begin(); itr != players->end(); ++itr)
    {
        Player* pPlayer = sObjectMgr.GetPlayer(*itr);
        if (pPlayer && pPlayer->IsInWorld())
        {
            rolesMap.insert(std::make_pair(pPlayer->GetObjectGuid(), pPlayer->GetLFGPlayerState()->GetRoles()));
            if (!pPlayer->GetLFGPlayerState()->HasSingleRole())
                hasMultiRoles = true;
        }
    }

    if (!hasMultiRoles)
        return;

    SetRoles(&rolesMap);
}

void LFGMgr::TryCompleteGroups()
{
    bool isGroupCompleted = false;  // we make only one group for iterations! not more!
    for (LFGQueueInfoMap::const_iterator itr = m_queueInfoMap.begin(); itr != m_queueInfoMap.end(); ++itr)
    {
        LFGDungeonEntry const* dungeon = itr->first;
        LFGQueueInfo const* info = &itr->second;
        LFGType type = LFGType(dungeon->type);

        if (type == LFG_TYPE_RAID || type == LFG_TYPE_NONE || type == LFG_TYPE_MAX) // the last two cases should never appear, but for save we continue
            continue;

        if (info->groupGuids.empty())
            continue;

        GuidSet groups = info->groupGuids;
        for (GuidSet::const_iterator groupsitr = groups.begin(); groupsitr != groups.end(); ++groupsitr)
        {
            Group* pGroup = sObjectMgr.GetGroup(*groupsitr);
            if (!pGroup)
                continue;

            if (pGroup->GetLFGGroupState()->GetState() != LFG_STATE_QUEUED)
                continue;

            DEBUG_LOG("LFGMgr:TryCompleteGroups: Try complete group %u  type %u state %u", pGroup->GetObjectGuid().GetCounter(), type, pGroup->GetLFGGroupState()->GetState());

            if (LFGMgr::IsGroupCompleted(pGroup))
            {
                GuidSet tmp;
                tmp.clear();
                DEBUG_LOG("LFGMgr:TryCompleteGroups: Try complete group %u  type %u, but his already completed!", pGroup->GetObjectGuid().GetCounter(), type);
                CompleteGroup(pGroup, tmp, dungeon);
                isGroupCompleted = true;
                break;
            }

            if (info->playerGuids.empty())
            {
                DEBUG_LOG("LFGMgr:TryCompleteGroups: Try complete group %u  type %u state %u, but we have no players", pGroup->GetObjectGuid().GetCounter(), type, pGroup->GetLFGGroupState()->GetState());
                continue;
            }

            GuidSet applicants;
            for (GuidSet::const_iterator iter = info->playerGuids.begin(); iter != info->playerGuids.end(); ++iter)
            {
                Player* pPlayer = sObjectMgr.GetPlayer(*iter);
                if (!pPlayer)
                    continue;

                if (pPlayer->GetLFGPlayerState()->GetState() != LFG_STATE_QUEUED)
                    continue;

                DEBUG_LOG("LFGMgr:TryCompleteGroups: Try complete group %u with player %u, type %u state %u", pGroup->GetObjectGuid().GetCounter(),pPlayer->GetObjectGuid().GetCounter(), type, pGroup->GetLFGGroupState()->GetState());

                applicants.insert(pPlayer->GetObjectGuid());
                if (TryAddMembersToGroup(pGroup, &applicants))
                {
                    if (LFGMgr::IsGroupCompleted(pGroup, applicants.size()))
                    {
                        CompleteGroup(pGroup, applicants, dungeon);
                        isGroupCompleted = true;
                    }
                }
                else
                    applicants.erase(pPlayer->GetObjectGuid());

                if (isGroupCompleted)           // jump out the player queue
                    break;
            }
            if (isGroupCompleted)               // jump out the group queue
                break;
        }
    }
}

bool LFGMgr::TryAddMembersToGroup(Group* pGroup, GuidSet const* players)
{
    if (!pGroup || players->empty())
        return false;

    for (GuidSet::const_iterator itr = players->begin(); itr != players->end(); ++itr)
    {
        Player* pPlayer = sObjectMgr.GetPlayer(*itr);
        if (!pPlayer || !pPlayer->IsInWorld())
            return false;

        if (!LFGMgr::IsInSameTeam(pGroup, pPlayer))
           return false;

        if (LFGMgr::HasIgnoreState(pGroup, pPlayer->GetObjectGuid()))
           return false;

        if (!CheckRoles(pGroup, pPlayer))
           return false;

        /*
        if (LFGProposal* pProposal = group->GetLFGState()->GetProposal())
        {
            if (pProposal->IsDecliner(player->GetObjectGuid()))
               return false;
        }
        */
    }
    return true;
}

// no GuidSet Pointer because with this copy of GuidSet we make maybe a Proposal
void LFGMgr::CompleteGroup(Group* pGroup, GuidSet players, LFGDungeonEntry const* dungeon)
{
    DEBUG_LOG("LFGMgr:CompleteGroup: Try complete group %u with %u players", pGroup->GetObjectGuid().GetCounter(), players.size());
    LFGDungeonEntry const* chosenDungeon;
    // if we have already a chosen dungeon, then use it. This Group is in Offer Continue
    if (pGroup->GetLFGGroupState()->GetChosenDungeon())
    {
        chosenDungeon = pGroup->GetLFGGroupState()->GetChosenDungeon();
    }
    else
    {
        chosenDungeon = dungeon;
    }
    if (chosenDungeon)
    {
        SetGroupRoles(pGroup, &players);
        uint32 ID = CreateProposal(chosenDungeon, pGroup, players);
        DEBUG_LOG("LFGMgr:CompleteGroup: dungeons for group %u with %u players found, created proposal %u", pGroup->GetObjectGuid().GetCounter(), players.size(), ID);
    }
}

bool LFGMgr::TryCreateGroup()
{
    bool groupCreated = false;
    for (LFGQueueInfoMap::const_iterator infoMapitr = m_queueInfoMap.begin(); infoMapitr != m_queueInfoMap.end(); ++infoMapitr)
    {
        LFGDungeonEntry const* dungeon = infoMapitr->first;
        LFGQueueInfo const* info = &infoMapitr->second;
        LFGType type = LFGType(dungeon->type);

        GuidSet players = info->playerGuids;
        if (players.empty())
            continue;

        if (!LFGMgr::IsGroupCompleted(NULL, players.size()))
            continue;

        if (type == LFG_TYPE_RAID || type == LFG_TYPE_NONE || type == LFG_TYPE_MAX) // the last two cases should never appear, but for save we continue
            continue;

        DEBUG_LOG("LFGMgr:TryCreateGroup: Try create group  with %u players", players.size());

        GuidSet newGroup;
        for (GuidSet::const_iterator playersitr = players.begin(); playersitr != players.end(); ++playersitr)
        {
            ObjectGuid playerGuid = *playersitr;
            Player* pPlayer = sObjectMgr.GetPlayer(playerGuid);

            if (pPlayer && pPlayer->IsInWorld())
            {
                if (pPlayer->GetLFGPlayerState()->GetState() != LFG_STATE_QUEUED)
                {
                    DEBUG_LOG("LFGMgr::TryCreateGroup: Player is not in State Queue. Actual state: %u", pPlayer->GetLFGPlayerState()->GetState());
                    continue;
                }

                bool checkPassed = true;
                LFGRolesMap rolesMap;
                for (GuidSet::const_iterator groupitr = newGroup.begin(); groupitr != newGroup.end(); ++groupitr)
                {
                    ObjectGuid groupMemberGuid = *groupitr;
                    Player* pGroupMember = sObjectMgr.GetPlayer(groupMemberGuid);
                    if (pGroupMember && pGroupMember->IsInWorld())
                    {
                        if (!LFGMgr::IsInSameTeam(pGroupMember, pPlayer))
                        {
                            checkPassed = false;
                            break;
                        }
                        else if (LFGMgr::HasIgnoreState(pGroupMember, playerGuid))
                        {
                            checkPassed = false;
                            break;
                        }
                        else
                            rolesMap.insert(std::make_pair(pGroupMember->GetObjectGuid(), pGroupMember->GetLFGPlayerState()->GetRoles()));
                    }
                }
                if (!checkPassed)
                    continue;

                rolesMap.insert(std::make_pair(pPlayer->GetObjectGuid(), pPlayer->GetLFGPlayerState()->GetRoles()));

                if (!CheckRoles(&rolesMap))
                   continue;

                newGroup.insert(playerGuid);

                if (LFGMgr::IsGroupCompleted(NULL, newGroup.size()))
                   groupCreated = true;

                if (!groupCreated)
                   continue;

                SetRoles(&rolesMap);
                break;
            }
        }
        if (groupCreated)
        {
            CreateProposal(dungeon, NULL, newGroup);
            return true;
        }
    }
    return false;
}

void LFGMgr::UpdateQueueStatus()
{
    for (LFGQueueInfoMap::iterator infoMapitr = m_queueInfoMap.begin(); infoMapitr != m_queueInfoMap.end(); ++infoMapitr)
    {
        LFGDungeonEntry const* dungeon = infoMapitr->first;
        LFGQueueInfo* info = &infoMapitr->second;
        GuidSet groupGuids = info->groupGuids;
        GuidSet playerGuids = info->playerGuids;
        info->ResetStats();
        for (GuidSet::const_iterator groupsitr = groupGuids.begin(); groupsitr != groupGuids.end(); ++groupsitr)
        {
            Group* pGroup = sObjectMgr.GetGroup(*groupsitr);
            if (pGroup)
            {
                for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
                {
                    Player* pGroupMember = itr->getSource();
                    time_t timeInQueue = pGroupMember->GetLFGPlayerState()->GetWaitTime();
                    if (pGroupMember && pGroupMember->IsInWorld())
                    {
                        LFGPlayerState* state = pGroupMember->GetLFGPlayerState();
                        if (state->HasRole(ROLE_TANK))
                        {
                            info->tanks++;
                            info->tanksTime += timeInQueue;
                        }
                        else if (state->HasRole(ROLE_HEALER))
                        {
                            info->healers++;
                            info->healersTime += timeInQueue;
                        }
                        else if (state->HasRole(ROLE_DAMAGE))
                        {
                            info->damagers++;
                            info->damagersTime += timeInQueue;
                        }
                        else
                        {
                            DEBUG_LOG("LFGMgr::UpdateQueueStatus:Groupmember %u has no Role, this should not appear!", pGroupMember->GetObjectGuid().GetCounter());
                        }
                    }
                }
            }
        }
        for (GuidSet::const_iterator playersitr = playerGuids.begin(); playersitr != playerGuids.end(); ++playersitr)
        {
            Player* pPlayer = sObjectMgr.GetPlayer(*playersitr);
            if (pPlayer && pPlayer->IsInWorld())
            {
                time_t timeInQueue = pPlayer->GetLFGPlayerState()->GetWaitTime();
                LFGPlayerState* state = pPlayer->GetLFGPlayerState();
                if (state->HasRole(ROLE_TANK))
                {
                    info->tanks++;
                    info->tanksTime += timeInQueue;
                }
                else if (state->HasRole(ROLE_HEALER))
                {
                    info->healers++;
                    info->healersTime += timeInQueue;
                }
                else if (state->HasRole(ROLE_DAMAGE))
                {
                    info->damagers++;
                    info->damagersTime += timeInQueue;
                }
                else
                {
                    DEBUG_LOG("LFGMgr::UpdateQueueStatus:Player %u has no Role, this should not appear!", pPlayer->GetObjectGuid().GetCounter());
                }
            }
        }
        DEBUG_LOG("LFGMgr::UpdateQueueStatus: dungeonId %u tanks %u, healers %u, damagers %u", dungeon->ID, info->tanks, info->healers, info->damagers);
    }
}

void LFGMgr::SendStatistic()
{
    for (LFGQueueInfoMap::const_iterator infoMapitr = m_queueInfoMap.begin(); infoMapitr != m_queueInfoMap.end(); ++infoMapitr)
    {
        LFGDungeonEntry const* dungeon = infoMapitr->first;
        LFGQueueInfo const* info = &infoMapitr->second;
        GuidSet groupGuids = info->groupGuids;
        GuidSet playerGuids = info->playerGuids;
        DEBUG_LOG("LFGMgr::SendStatistic: DungeonID %u with %u Groups and %u Players", dungeon->ID, groupGuids.size(), playerGuids.size());
        switch (LFGType(dungeon->type))
        {
            case LFG_TYPE_RAID:
            case LFG_TYPE_QUEST:
            case LFG_TYPE_ZONE:
                continue;
            case LFG_TYPE_DUNGEON:
            case LFG_TYPE_HEROIC_DUNGEON:
            case LFG_TYPE_RANDOM_DUNGEON:
                break;
            case LFG_TYPE_NONE:
            default:
                DEBUG_LOG("LFGMgr::SendStatistic: DungeonId: %u has no real type", dungeon->ID);
                return;
        }
        for (GuidSet::const_iterator groupitr = groupGuids.begin(); groupitr != groupGuids.end(); ++groupitr)
        {
            ObjectGuid groupGuid = *groupitr;
            Group* pGroup = sObjectMgr.GetGroup(groupGuid);
            if (!pGroup)
                continue;
            for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* pGroupMember = itr->getSource();
                if (pGroupMember && pGroupMember->IsInWorld())
                    pGroupMember->GetSession()->SendLfgQueueStatus(dungeon);
            }
        }
        for (GuidSet::const_iterator playeritr = playerGuids.begin(); playeritr != playerGuids.end(); ++playeritr)
        {
            ObjectGuid playerGuid = *playeritr;
            Player* pPlayer = sObjectMgr.GetPlayer(playerGuid);
            if (!pPlayer || !pPlayer->IsInWorld())
                continue;
            pPlayer->GetSession()->SendLfgQueueStatus(dungeon);
        }
    }

    /*
     * For LFG_TYPE_DUNGEON, LFG_TYPE_HEROIC_DUNGEON Players can select more Dungeons,
     * but send all information for selected Dungeon should be false.
     * Maybe the Client filters for the fullest Dungeon, then change the switch case in the loop before.
     *
     * LFG_TYPE_RAID: case LFG_TYPE_QUEST: case LFG_TYPE_ZONE:
     */
    //TODO: more
}

void LFGMgr::UpdateLFRGroups()
{
    for (LFGQueueInfoMap::const_iterator infoMapitr = m_queueInfoMap.begin(); infoMapitr != m_queueInfoMap.end(); ++infoMapitr)
    {
        LFGDungeonEntry const* dungeon = infoMapitr->first;
        LFGQueueInfo const* info = &infoMapitr->second;
        GuidSet groupGuids = info->groupGuids;

        if (dungeon->type == LFG_TYPE_RAID)
            continue;

        if (groupGuids.empty())
            continue;
        for (GuidSet::const_iterator itr = groupGuids.begin(); itr != groupGuids.end(); ++itr)
        {
            ObjectGuid groupGuid = *itr;
            Group* pGroup = sObjectMgr.GetGroup(groupGuid);
            if (!pGroup || !pGroup->isLFRGroup())
                continue;

            if (pGroup->GetLFGGroupState()->GetState() != LFG_STATE_LFR)
                continue;

            if (!LFGMgr::IsGroupCompleted(pGroup))
                continue;

            pGroup->GetLFGGroupState()->SetChosenDungeon(dungeon);
            DEBUG_LOG("LFGMgr::UpdateLFRGroup: %u set real dungeon to %u.", pGroup->GetObjectGuid().GetCounter(), dungeon->ID);

            Player* pLeader = sObjectMgr.GetPlayer(pGroup->GetLeaderGuid());
            if (pLeader && pLeader->GetMapId() != dungeon->map)
            {
                pGroup->SetDungeonDifficulty(Difficulty(dungeon->difficulty));
                pGroup->GetLFGGroupState()->SetStatus(LFG_STATUS_NOT_SAVED);
                pGroup->SendUpdate();
            }

            uint8 tpnum = 0;
            for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* pGroupMember = itr->getSource();
                if (pGroupMember && pGroupMember->IsInWorld())
                {
                    if (pGroupMember->GetLFGPlayerState()->GetState() < LFG_STATE_IN_DUNGEON)
                    {
                        AddMemberToLFDGroup(pGroupMember->GetObjectGuid());
                        pGroupMember->GetLFGPlayerState()->SetState(LFG_STATE_IN_DUNGEON);
                        ++tpnum;
                    }
                }
            }
            if (tpnum)
                Teleport(pGroup, false);
            RemoveFromQueue(pGroup->GetObjectGuid());
        }
    }
}

void LFGMgr::AddMemberToLFDGroup(ObjectGuid guid)
{
    Player* pPlayer = sObjectMgr.GetPlayer(guid);

    if (!pPlayer || !pPlayer->IsInWorld())
        return;

    Group* pGroup = pPlayer->GetGroup();

    if (!pGroup)
        return;

    pGroup->SetGroupRoles(guid, pPlayer->GetLFGPlayerState()->GetRoles());
    RemoveFromQueue(pPlayer->GetObjectGuid());

    pPlayer->GetLFGPlayerState()->SetState(pGroup->GetLFGGroupState()->GetState());
}

void LFGMgr::RemoveMemberFromLFDGroup(Group* pGroup, ObjectGuid guid)
{
    Player* pPlayer = sObjectMgr.GetPlayer(guid);

    if (!pPlayer || !pPlayer->IsInWorld())
        return;

    if (pPlayer->HasAura(LFG_SPELL_DUNGEON_COOLDOWN) && !sWorld.getConfig(CONFIG_BOOL_LFG_DEBUG_ENABLE))
        pPlayer->CastSpell(pPlayer,LFG_SPELL_DUNGEON_DESERTER,true);


    if (!pGroup || !pGroup->isLFDGroup())
    {
        pPlayer->GetLFGPlayerState()->Clear();
        return;
    }

    if (pPlayer->GetLFGPlayerState()->GetState() > LFG_STATE_QUEUED)
        Teleport(pPlayer, true);
    else if (pGroup && pGroup->GetLFGGroupState()->GetState() > LFG_STATE_QUEUED)
        Teleport(pPlayer, true);

    if (pGroup && pGroup->isLFGGroup() && pGroup->GetMembersCount() > 1)
    {
        if (pGroup->GetLFGGroupState()->GetState() > LFG_STATE_LFG
            && pGroup->GetLFGGroupState()->GetState() < LFG_STATE_FINISHED_DUNGEON)
        {
            OfferContinue(pGroup);
        }
    }

    Leave(pPlayer);
    pPlayer->GetLFGPlayerState()->Clear();
}

/*
 * Called when Group reached an encounter.
 * This function removes the dungeon cooldown Aura.
 */
void LFGMgr::DungeonEncounterReached(Group* pGroup)
{
    if (!pGroup)
        return;

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pGroupMember = itr->getSource();
        if (pGroupMember && pGroupMember->IsInWorld())
            pGroupMember->RemoveAurasDueToSpell(LFG_SPELL_DUNGEON_COOLDOWN);
    }
}

/*
 * returns LFGType of all given dungeons, if dungeons have different types then LFG_TYPE_NONE
 */
LFGType LFGMgr::GetAndCheckLFGType(LFGDungeonSet const* dungeons)
{
    if (dungeons->empty())
        return LFG_TYPE_NONE;
    LFGDungeonSet::const_iterator itr = dungeons->begin();
    LFGDungeonEntry const* dungeon = *itr;
    LFGType resultType = LFGType(dungeon->type);
    while (itr != dungeons->end())
    {
        LFGDungeonEntry const* dungeon = *itr;
        if (LFGType(dungeon->type) != resultType)
            return LFG_TYPE_NONE;
        ++itr;
    }
    return resultType;
}

/*
 * Checks whether a group is complete
 */
bool LFGMgr::IsGroupCompleted(Group const* pGroup, uint8 uiAddMembers)
{
    if (!pGroup)
    {
        if (sWorld.getConfig(CONFIG_BOOL_LFG_DEBUG_ENABLE) && uiAddMembers > 2)
            return true;
        else if ( uiAddMembers >= MAX_GROUP_SIZE)
            return true;
        else
            return false;
    }

    if (sWorld.getConfig(CONFIG_BOOL_LFG_DEBUG_ENABLE) && (pGroup->GetMembersCount() + uiAddMembers > 2))
        return true;

    if (pGroup->isRaidGroup())
    {
        switch (pGroup->GetDifficulty(true))
        {
            case RAID_DIFFICULTY_10MAN_NORMAL:
            case RAID_DIFFICULTY_10MAN_HEROIC:
                if (pGroup->GetMembersCount() + uiAddMembers >= 10)
                    return true;
                break;
            case RAID_DIFFICULTY_25MAN_NORMAL:
            case RAID_DIFFICULTY_25MAN_HEROIC:
                if (pGroup->GetMembersCount() + uiAddMembers >= 25)
                    return true;
                break;
            default:
                return false;
                break;
        }
    }
    else if (pGroup->GetMembersCount() + uiAddMembers >= MAX_GROUP_SIZE)
        return true;

    return false;
}

/*
 * This function returns a random Dungeon from list. If list is empty we return NULL.
 */
LFGDungeonEntry const* LFGMgr::SelectRandomDungeonFromList(LFGDungeonSet const* dungeons)
{
    if (dungeons->empty())
    {
        DEBUG_LOG("LFGMgr::SelectRandomDungeonFromList: cannot select dungeons from empty list!");
        return NULL;
    }

    LFGDungeonEntry const* randomDungeon = *dungeons->begin();
    if (dungeons->size() > 1)
    {
        uint32 rand = urand(0, dungeons->size() - 1);
        uint32 key = 0;
        for (LFGDungeonSet::const_iterator itr = dungeons->begin(); itr != dungeons->end(); ++itr)
        {
            LFGDungeonEntry const* dungeon = *itr;
            if (!dungeon)
                continue;
            if (key == rand)
                return dungeon;
            else ++key;
        }
    }
    return randomDungeon;
}

/*
 * return true if dungeon type = 6 (LFG_TYPE_RANDOM_DUNGEON), else false
 */
bool LFGMgr::IsRandomDungeon(LFGDungeonEntry const* dungeon)
{
    if (!dungeon)
        return false;

    return dungeon->type == LFG_TYPE_RANDOM_DUNGEON;
}

bool LFGMgr::HasIgnoreState(Player* pPlayer, ObjectGuid checkTarget)
{
    if (!pPlayer)
        return false;

     return pPlayer->GetSocial()->HasIgnore(checkTarget);
}

bool LFGMgr::HasIgnoreState(Group* pGroup, ObjectGuid checkTarget)
{
    if (!pGroup)
        return false;

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        if (Player* pGroupMember = itr->getSource())
            if (pGroupMember->GetSocial()->HasIgnore(checkTarget))
                return true;
    }

    return false;
}

bool LFGMgr::IsInSameTeam(Player* pPlayer1, Player* pPlayer2)
{
    if (!pPlayer1 || !pPlayer2)
        return true;

    if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP))
        return true;

    return pPlayer1->GetTeam() == pPlayer2->GetTeam();
}

bool LFGMgr::IsInSameTeam(Group* pGroup, Player* pPlayer)
{
    if (!pGroup || !pPlayer)
        return true;

    if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP))
        return true;

    Player* pLeader = sObjectMgr.GetPlayer(pGroup->GetLeaderGuid());
    if (pLeader)
    {
        if (pLeader->GetTeam() == pPlayer->GetTeam())
            return true;
    }
    return false;
}


void LFGMgr::SheduleEvent()
{
    if (m_eventList.empty())
        return;

    for (LFGEventList::iterator itr = m_eventList.begin(); itr != m_eventList.end(); ++itr)
    {
        // we run only one event for tick!!!
        if (!itr->IsActive())
            continue;
        else
        {
            DEBUG_LOG("LFGMgr::SheduleEvent guid %u type %u",itr->guid.GetCounter(), itr->type);
            switch (itr->type)
            {
                case LFG_EVENT_TELEPORT_PLAYER:
                    {
                        Player* pPlayer = sObjectMgr.GetPlayer(itr->guid);
                        if (pPlayer)
                            Teleport(pPlayer, bool(itr->eventParm));
                    }
                    break;
                case LFG_EVENT_TELEPORT_GROUP:
                    {
                        Group* pGroup = sObjectMgr.GetGroup(itr->guid);
                        if (pGroup)
                            Teleport(pGroup, bool(itr->eventParm));
                    }
                    break;
                case LFG_EVENT_NONE:
                default:
                    break;
            }
            m_eventList.erase(itr);
            break;
        }
    }
}

void LFGMgr::AddEvent(ObjectGuid guid, LFGEventType type, time_t delay, uint8 param)
{
    DEBUG_LOG("LFGMgr::AddEvent guid %u type %u",guid.GetCounter(), type);
    LFGEvent event = LFGEvent(type,guid,param);
    m_eventList.push_back(event);
    m_eventList.rbegin()->Start(delay);
}

void LFGMgr::LoadLFDGroupPropertiesForPlayer(Player* pPlayer)
{
    if (!pPlayer || !pPlayer->IsInWorld())
        return;

    Group* pGroup = pPlayer->GetGroup();
    if (!pGroup)
        return;

    pPlayer->GetLFGPlayerState()->SetRoles(pGroup->GetGroupRoles(pPlayer->GetObjectGuid()));
    if(sWorld.getConfig(CONFIG_BOOL_RESTRICTED_LFG_CHANNEL))
        pPlayer->JoinLFGChannel();

    switch (pGroup->GetLFGGroupState()->GetState())
    {
        case LFG_STATE_NONE:
        case LFG_STATE_FINISHED_DUNGEON:
        {
            pPlayer->GetSession()->SendLfgUpdateParty(LFG_UPDATETYPE_JOIN_PROPOSAL, pGroup->GetLFGGroupState()->GetType());
            break;
        }
        case LFG_STATE_IN_DUNGEON:
        {
            if (pGroup->GetLFGGroupState()->GetType() == LFG_TYPE_RAID)
                break;
            pPlayer->GetSession()->SendLfgUpdateParty(LFG_UPDATETYPE_GROUP_FOUND, pGroup->GetLFGGroupState()->GetType());
            break;
        }
        default:
           break;
    }
}

void LFGMgr::OnPlayerEnterMap(Player* pPlayer, Map* pMap)
{
    if (!pPlayer || !pPlayer->IsInWorld() || !pMap)
        return;

    Group* pGroup = pPlayer->GetGroup();

    if (!pGroup || !pGroup->isLFDGroup())
        return;

    if (pMap->IsDungeon() && pGroup->isLFGGroup())
        pPlayer->CastSpell(pPlayer,LFG_SPELL_LUCK_OF_THE_DRAW,true);
    else if (pMap->IsRaid() && pGroup->isLFRGroup() && sWorld.getConfig(CONFIG_BOOL_LFR_ENABLE))
        pPlayer->CastSpell(pPlayer,LFG_SPELL_LUCK_OF_THE_DRAW,true);
    else
        pPlayer->RemoveAurasDueToSpell(LFG_SPELL_LUCK_OF_THE_DRAW);
}

void LFGMgr::OnPlayerLeaveMap(Player* pPlayer, Map* pMap)
{
    if (!pPlayer || !pPlayer->IsInWorld() || !pMap)
        return;

    Group* pGroup = pPlayer->GetGroup();

    if (pPlayer->HasAura(LFG_SPELL_LUCK_OF_THE_DRAW))
    {
        if (!pGroup || !pGroup->isLFDGroup())
            pPlayer->RemoveAurasDueToSpell(LFG_SPELL_LUCK_OF_THE_DRAW);
    }
}

