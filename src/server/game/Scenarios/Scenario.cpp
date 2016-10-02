/*
 * Copyright (C) 2008-2015 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Scenario.h"
#include "Player.h"
#include "ScenarioMgr.h"
#include "InstanceSaveMgr.h"
#include "ObjectMgr.h"
#include "AchievementPackets.h"
#include "ScenarioPackets.h"

Scenario::Scenario(ScenarioData const* scenarioData) : _data(scenarioData), _isComplete(false), _currentStepIndex(0), _currentstep(nullptr)
{
    ASSERT(_data);

    if (ScenarioStepEntry const* step = GetFirstStep())
        SetStep(step);
    else
        TC_LOG_ERROR("scenario", "Scenario::Scenario: Could not launch Scenario (id: %u), found no valid scenario step");
}

Scenario::~Scenario()
{
    WorldPackets::Scenario::ScenarioState scenarioState;
    BuildScenarioState(&scenarioState, false, true);
    SendPacket(scenarioState.Write());

    _players.clear();
}

void Scenario::Reset()
{
    CriteriaHandler::Reset();
    SetStep(GetFirstStep());
}

void Scenario::CompleteStep(ScenarioStepEntry const* step)
{
    if (IsComplete())
        return;

    if (Quest const* quest = sObjectMgr->GetQuestTemplate(step->QuestRewardID))
        for (auto guid : _players)
            if (Player* player = ObjectAccessor::FindPlayer(guid))
                player->RewardQuest(quest, 0, nullptr, false);

    if (step->IsBonusObjective())
        return;

    if (CheckIfComplete())
    {
        CompleteScenario();
        return;
    }

    SetNextStep();
}

void Scenario::CompleteScenario()
{
    if (IsComplete())
        return;

    _isComplete = true;

    WorldPackets::Scenario::ScenarioState scenarioState;
    BuildScenarioState(&scenarioState);
    SendPacket(scenarioState.Write());

    WorldPackets::Scenario::ScenarioCompleted scenarioCompleted;
    scenarioCompleted.ScenarioId = _data->Entry->ID;
    SendPacket(scenarioCompleted.Write());
}

void Scenario::SetStepByIndex(uint8 newStepIndex)
{
    auto stepItr = _data->Steps.find(newStepIndex);

    if (stepItr == _data->Steps.end())
    {
        TC_LOG_ERROR("scenario", "Scenario::SetStepByIndex: Scenario (id: %u, step: %u) could not determine new step.", _data->Entry->ID, newStepIndex);
        return;
    }

    _currentStepIndex = newStepIndex;
    SetStep(stepItr->second);
}

void Scenario::SetStep(ScenarioStepEntry const* step)
{
    ASSERT(step);
    _currentstep = step;

    WorldPackets::Scenario::ScenarioState scenarioState;
    BuildScenarioState(&scenarioState);
    SendPacket(scenarioState.Write());
}

void Scenario::OnPlayerEnter(Player* player)
{
    if (IsComplete())
        return;

    _players.insert(player->GetGUID());

    for (auto critItr = _criteriaProgress.begin(); critItr != _criteriaProgress.end(); ++critItr)
    {
        WorldPackets::Achievement::CriteriaDeleted criteriaDeleted;
        criteriaDeleted.CriteriaID = critItr->first;
        player->SendDirectMessage(criteriaDeleted.Write());
    }

    SendScenarioState(player, true);
}

void Scenario::OnPlayerExit(Player* player)
{
    _players.erase(player->GetGUID());

    SendScenarioState(player, false, true);
}

bool Scenario::CheckIfComplete()
{
    if (IsComplete())
        return true;

    for (auto step : _data->Steps)
    {
        if (step.second->IsBonusObjective())
            continue;

        if (!IsStepCompleted(step.second))
            return false;
    }

    return true;
}

bool Scenario::IsStepCompleted(ScenarioStepEntry const* step)
{
    CriteriaTree const* criteriaTree = sCriteriaMgr->GetCriteriaTree(step->CriteriaTreeID);
    if (!criteriaTree)
    {
        TC_LOG_ERROR("scenario", "Scenario::IsStepCompleted: Could not find Criteria Tree (id: %u) for Scenario (id: %u, step: %u)", step->CriteriaTreeID, step->StepIndex, step->ScenarioID);
        return false;
    }

    return IsCompletedCriteriaTree(criteriaTree);
}

void Scenario::SendCriteriaUpdate(Criteria const* criteria, CriteriaProgress const* progress, uint32 timeElapsed, bool timedCompleted) const
{
    WorldPackets::Scenario::ScenarioProgressUpdate progressUpdate;
    WorldPackets::Achievement::CriteriaProgress criteriaProgress;
    criteriaProgress.Id = criteria->ID;
    criteriaProgress.Quantity = progress->Counter;
    criteriaProgress.Player = progress->PlayerGUID;
    criteriaProgress.Date = progress->Date;
    if (criteria->Entry->StartTimer)
        criteriaProgress.Flags = timedCompleted ? 1 : 0;

    criteriaProgress.TimeFromStart = timeElapsed;
    criteriaProgress.TimeFromCreate = 0;

    progressUpdate.criteriaProgress = criteriaProgress;
    SendPacket(progressUpdate.Write());
}

void Scenario::SendCriteriaProgressRemoved(uint32 criteriaId)
{
}

bool Scenario::CanUpdateCriteriaTree(Criteria const* /*criteria*/, CriteriaTree const* tree, Player* /*referencePlayer*/) const
{
    ScenarioStepEntry const* step = GetStep();
    if (!tree->ScenarioStep || !step || tree->ScenarioStep->ScenarioID != step->ScenarioID)
        return false;

    if (tree->ScenarioStep->Flags & SCENARIO_STEP_FLAG_BONUS_OBJECTIVE)
        return !(tree->ScenarioStep->Flags & SCENARIO_STEP_FLAG_SCENARIO_NOT_DONE);

    return step->StepIndex == tree->ScenarioStep->StepIndex;
}

bool Scenario::CanCompleteCriteriaTree(CriteriaTree const* tree)
{
    return CanUpdateCriteriaTree(nullptr, tree, nullptr);
}

void Scenario::CompletedCriteriaTree(CriteriaTree const* tree, Player* referencePlayer)
{
    if (!tree->ScenarioStep || !CheckIfComplete())
        return;

    if (!IsStepCompleted(tree->ScenarioStep))
        return;

    CompleteStep(tree->ScenarioStep);
}

void Scenario::SendPacket(WorldPacket const* data) const
{
    for (auto guid : _players)
        if (Player* player = ObjectAccessor::FindPlayer(guid))
            player->SendDirectMessage(data);
}

void Scenario::BuildScenarioState(WorldPackets::Scenario::ScenarioState* scenarioState, bool initial /*= false*/, bool remove /*= false*/)
{
    scenarioState->ScenarioId = _data->Entry->ID;

    if (IsComplete() || remove)
        scenarioState->CurrentStep = -1;
    else if (ScenarioStepEntry const* step = GetStep())
        scenarioState->CurrentStep = step->ID;

    scenarioState->CriteriaProgress = GetCriteriasProgress();
    scenarioState->AvailableObjectives = GetBonusObjectivesData();

    for (auto step : _data->Steps)
    {
        if (step.second->IsBonusObjective())
            continue;

        if (step.second->StepIndex > _currentStepIndex)
            continue;

        scenarioState->TraversedSteps.push_back(step.second->ID);
    }

    // Sent at first state, todo: figure out why
    if (initial)
    {
        for (uint8 i = 0; i < 4; ++i)
        {
            WorldPackets::Scenario::BonusObjectiveData objectiveData;
            objectiveData.Id = 0;
            objectiveData.ObjectiveCompleted = true;
            scenarioState->CompletedObjectives.push_back(objectiveData);
        }
    }

    scenarioState->ScenarioCompleted = IsComplete();
}

ScenarioStepEntry const* Scenario::GetFirstStep() const
{
    auto stepItr = _data->Steps.find(0);
    if (stepItr != _data->Steps.end())
        return stepItr->second;

    return nullptr;
}

void Scenario::SendScenarioState(Player* player, bool initial /*= false*/, bool remove /*= false*/)
{
    WorldPackets::Scenario::ScenarioState scenarioState;
    BuildScenarioState(&scenarioState, initial, remove);
    player->SendDirectMessage(scenarioState.Write());
}

std::vector<WorldPackets::Scenario::BonusObjectiveData> Scenario::GetBonusObjectivesData()
{
    std::vector<WorldPackets::Scenario::BonusObjectiveData> bonusObjectivesData;
    for (auto itr = _data->Steps.begin(); itr != _data->Steps.end(); ++itr)
    {
        if (!(itr->second->Flags & SCENARIO_STEP_FLAG_BONUS_OBJECTIVE))
            continue;

        CriteriaTree const* tree = sCriteriaMgr->GetCriteriaTree(itr->second->CriteriaTreeID);
        if (tree)
        {
            WorldPackets::Scenario::BonusObjectiveData bonusObjectiveData;
            bonusObjectiveData.Id = itr->second->ID;
            bonusObjectiveData.ObjectiveCompleted = IsCompletedCriteriaTree(tree);
            bonusObjectivesData.push_back(bonusObjectiveData);
        }
    }

    return bonusObjectivesData;
}

std::vector<WorldPackets::Achievement::CriteriaProgress> Scenario::GetCriteriasProgress()
{
    std::vector<WorldPackets::Achievement::CriteriaProgress> criteriasProgress;

    if (!_criteriaProgress.empty())
    {
        for (auto critItr = _criteriaProgress.begin(); critItr != _criteriaProgress.end(); ++critItr)
        {
            WorldPackets::Achievement::CriteriaProgress criteriaProgress;
            criteriaProgress.Id = critItr->first;
            criteriaProgress.Quantity = critItr->second.Counter;
            criteriaProgress.Date = critItr->second.Date;
            criteriaProgress.Player = critItr->second.PlayerGUID;
            criteriasProgress.push_back(criteriaProgress);
        }
    }

    return criteriasProgress;
}

std::string Scenario::GetOwnerInfo() const
{
    return std::string();
}

CriteriaList const & Scenario::GetCriteriaByType(CriteriaTypes type) const
{
    return sCriteriaMgr->GetScenarioCriteriaByType(type);
}

void Scenario::SendBootPlayer(Player* player) const
{
    player->SendDirectMessage(WorldPackets::Scenario::ScenarioBoot().Write());
}