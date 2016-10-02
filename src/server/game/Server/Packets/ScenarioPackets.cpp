/*
* Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
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

#include "ScenarioPackets.h"

WorldPacket const* WorldPackets::Scenario::ScenarioState::Write()
{
    _worldPacket << int32(ScenarioId);
    _worldPacket << int32(CurrentStep);
    _worldPacket << int32(DifficultyId);
    _worldPacket << int32(WaveCurrent);
    _worldPacket << int32(WaveMax);
    _worldPacket << int32(TimerDuration);
    _worldPacket << int32(CriteriaProgress.size());
    _worldPacket << uint32(AvailableObjectives.size()); // guessed
    _worldPacket << int32(TraversedSteps.size());
    _worldPacket << uint32(CompletedObjectives.size()); // guessed

    for (uint32 step : TraversedSteps)
        _worldPacket << step;

    _worldPacket.WriteBit(ScenarioCompleted);
    _worldPacket.FlushBits();

    for (uint32 i = 0; i < CriteriaProgress.size(); ++i)
    {
        _worldPacket << uint32(CriteriaProgress[i].Id);
        _worldPacket << uint64(CriteriaProgress[i].Quantity);
        _worldPacket << CriteriaProgress[i].Player;
        _worldPacket.AppendPackedTime(CriteriaProgress[i].Date);
        _worldPacket << uint32(CriteriaProgress[i].TimeFromStart);
        _worldPacket << uint32(CriteriaProgress[i].TimeFromCreate);
        _worldPacket.WriteBits(CriteriaProgress[i].Flags, 4);
        _worldPacket.FlushBits();
    }

    for (uint32 i = 0; i < AvailableObjectives.size(); ++i)
    {
        _worldPacket << int32(AvailableObjectives[i].Id);
        _worldPacket.WriteBit(AvailableObjectives[i].ObjectiveCompleted);
        _worldPacket.FlushBits();
    }

    for (uint32 i = 0; i < CompletedObjectives.size(); ++i)
    {
        _worldPacket << int32(CompletedObjectives[i].Id);
        _worldPacket.WriteBit(CompletedObjectives[i].ObjectiveCompleted);
        _worldPacket.FlushBits();
    }

    return &_worldPacket;
}

WorldPacket const* WorldPackets::Scenario::ScenarioProgressUpdate::Write()
{
    _worldPacket << int32(criteriaProgress.Id);
    _worldPacket << uint64(criteriaProgress.Quantity);
    _worldPacket << criteriaProgress.Player;
    _worldPacket.AppendPackedTime(criteriaProgress.Date);
    _worldPacket << uint32(criteriaProgress.TimeFromStart);
    _worldPacket << uint32(criteriaProgress.TimeFromCreate);

    _worldPacket.WriteBits(criteriaProgress.Flags, 4);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* WorldPackets::Scenario::ScenarioCompleted::Write()
{
    _worldPacket << ScenarioId;

    return &_worldPacket;
}
