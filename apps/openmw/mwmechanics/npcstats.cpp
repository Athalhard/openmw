
#include "npcstats.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>
#include <algorithm>

#include <boost/format.hpp>

#include <components/esm/loadskil.hpp>
#include <components/esm/loadclas.hpp>
#include <components/esm/loadgmst.hpp>
#include <components/esm/loadfact.hpp>

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/soundmanager.hpp"

MWMechanics::NpcStats::NpcStats()
: mDrawState (DrawState_Nothing)
, mBounty (0)
, mLevelProgress(0)
, mDisposition(0)
, mReputation(0)
, mWerewolfKills (0)
, mProfit(0)
, mAttackStrength(0.0f)
, mTimeToStartDrowning(20.0)
, mLastDrowningHit(0)
{
    mSkillIncreases.resize (ESM::Attribute::Length, 0);
}

MWMechanics::DrawState_ MWMechanics::NpcStats::getDrawState() const
{
    return mDrawState;
}

void MWMechanics::NpcStats::setDrawState (DrawState_ state)
{
    mDrawState = state;
}

float MWMechanics::NpcStats::getAttackStrength() const
{
    return mAttackStrength;
}

void MWMechanics::NpcStats::setAttackStrength(float value)
{
    mAttackStrength = value;
}

int MWMechanics::NpcStats::getBaseDisposition() const
{
    return mDisposition;
}

void MWMechanics::NpcStats::setBaseDisposition(int disposition)
{
    mDisposition = disposition;
}

const MWMechanics::SkillValue& MWMechanics::NpcStats::getSkill (int index) const
{
    if (index<0 || index>=ESM::Skill::Length)
        throw std::runtime_error ("skill index out of range");

    return (!mIsWerewolf ? mSkill[index] : mWerewolfSkill[index]);
}

MWMechanics::SkillValue& MWMechanics::NpcStats::getSkill (int index)
{
    if (index<0 || index>=ESM::Skill::Length)
        throw std::runtime_error ("skill index out of range");

    return (!mIsWerewolf ? mSkill[index] : mWerewolfSkill[index]);
}

const std::map<std::string, int>& MWMechanics::NpcStats::getFactionRanks() const
{
    return mFactionRank;
}

std::map<std::string, int>& MWMechanics::NpcStats::getFactionRanks()
{
    return mFactionRank;
}

bool MWMechanics::NpcStats::getExpelled(const std::string& factionID) const
{
    return mExpelled.find(Misc::StringUtils::lowerCase(factionID)) != mExpelled.end();
}

void MWMechanics::NpcStats::expell(const std::string& factionID)
{
    std::string lower = Misc::StringUtils::lowerCase(factionID);
    if (mExpelled.find(lower) == mExpelled.end())
    {
        std::string message = "#{sExpelledMessage}";
        message += MWBase::Environment::get().getWorld()->getStore().get<ESM::Faction>().find(factionID)->mName;
        MWBase::Environment::get().getWindowManager()->messageBox(message);
        mExpelled.insert(lower);
    }
}

void MWMechanics::NpcStats::clearExpelled(const std::string& factionID)
{
    mExpelled.erase(Misc::StringUtils::lowerCase(factionID));
}

bool MWMechanics::NpcStats::isSameFaction (const NpcStats& npcStats) const
{
    for (std::map<std::string, int>::const_iterator iter (mFactionRank.begin()); iter!=mFactionRank.end();
        ++iter)
        if (npcStats.mFactionRank.find (iter->first)!=npcStats.mFactionRank.end())
            return true;

    return false;
}

float MWMechanics::NpcStats::getSkillGain (int skillIndex, const ESM::Class& class_, int usageType,
    int level) const
{
    if (level<0)
        level = static_cast<int> (getSkill (skillIndex).getBase());

    const ESM::Skill *skill =
        MWBase::Environment::get().getWorld()->getStore().get<ESM::Skill>().find (skillIndex);

    float skillFactor = 1;

    if (usageType>=4)
        throw std::runtime_error ("skill usage type out of range");

    if (usageType>=0)
    {
        skillFactor = skill->mData.mUseValue[usageType];

        if (skillFactor<0)
            throw std::runtime_error ("invalid skill gain factor");

        if (skillFactor==0)
            return 0;
    }

    const MWWorld::Store<ESM::GameSetting> &gmst =
        MWBase::Environment::get().getWorld()->getStore().get<ESM::GameSetting>();

    float typeFactor = gmst.find ("fMiscSkillBonus")->getFloat();

    for (int i=0; i<5; ++i)
        if (class_.mData.mSkills[i][0]==skillIndex)
        {
            typeFactor = gmst.find ("fMinorSkillBonus")->getFloat();

            break;
        }

    for (int i=0; i<5; ++i)
        if (class_.mData.mSkills[i][1]==skillIndex)
        {
            typeFactor = gmst.find ("fMajorSkillBonus")->getFloat();

            break;
        }

    if (typeFactor<=0)
        throw std::runtime_error ("invalid skill type factor");

    float specialisationFactor = 1;

    if (skill->mData.mSpecialization==class_.mData.mSpecialization)
    {
        specialisationFactor = gmst.find ("fSpecialSkillBonus")->getFloat();

        if (specialisationFactor<=0)
            throw std::runtime_error ("invalid skill specialisation factor");
    }
    return 1.0 / ((level+1) * (1.0/skillFactor) * typeFactor * specialisationFactor);
}

void MWMechanics::NpcStats::useSkill (int skillIndex, const ESM::Class& class_, int usageType)
{
    // Don't increase skills as a werewolf
    if(mIsWerewolf)
        return;

    MWMechanics::SkillValue& value = getSkill (skillIndex);

    value.setProgress(value.getProgress() + getSkillGain (skillIndex, class_, usageType));

    if (value.getProgress()>=1)
    {
        // skill leveled up
        increaseSkill(skillIndex, class_, false);
    }
}

void MWMechanics::NpcStats::increaseSkill(int skillIndex, const ESM::Class &class_, bool preserveProgress)
{
    int base = getSkill (skillIndex).getBase();

    if (base >= 100)
        return;

    base += 1;

    // if this is a major or minor skill of the class, increase level progress
    bool levelProgress = false;
    for (int i=0; i<2; ++i)
        for (int j=0; j<5; ++j)
        {
            int skill = class_.mData.mSkills[j][i];
            if (skill == skillIndex)
                levelProgress = true;
        }

    mLevelProgress += levelProgress;

    // check the attribute this skill belongs to
    const ESM::Skill* skill =
        MWBase::Environment::get().getWorld ()->getStore ().get<ESM::Skill>().find(skillIndex);
    ++mSkillIncreases[skill->mData.mAttribute];

    // Play sound & skill progress notification
    /// \todo check if character is the player, if levelling is ever implemented for NPCs
    MWBase::Environment::get().getSoundManager ()->playSound ("skillraise", 1, 1);

    std::stringstream message;
    message << boost::format(MWBase::Environment::get().getWindowManager ()->getGameSettingString ("sNotifyMessage39", ""))
               % std::string("#{" + ESM::Skill::sSkillNameIds[skillIndex] + "}")
               % static_cast<int> (base);
    MWBase::Environment::get().getWindowManager ()->messageBox(message.str());

    if (mLevelProgress >= 10)
    {
        // levelup is possible now
        MWBase::Environment::get().getWindowManager ()->messageBox ("#{sLevelUpMsg}");
    }

    getSkill (skillIndex).setBase (base);
    if (!preserveProgress)
        getSkill(skillIndex).setProgress(0);
}

int MWMechanics::NpcStats::getLevelProgress () const
{
    return mLevelProgress;
}

void MWMechanics::NpcStats::levelUp()
{
    mLevelProgress -= 10;
    for (int i=0; i<ESM::Attribute::Length; ++i)
        mSkillIncreases[i] = 0;
}

int MWMechanics::NpcStats::getLevelupAttributeMultiplier(int attribute) const
{
    // Source: http://www.uesp.net/wiki/Morrowind:Level#How_to_Level_Up
    int num = mSkillIncreases[attribute];
    if (num <= 1)
        return 1;
    else if (num <= 4)
        return 2;
    else if (num <= 7)
        return 3;
    else if (num <= 9)
        return 4;
    else
        return 5;
}

void MWMechanics::NpcStats::flagAsUsed (const std::string& id)
{
    mUsedIds.insert (id);
}

bool MWMechanics::NpcStats::hasBeenUsed (const std::string& id) const
{
    return mUsedIds.find (id)!=mUsedIds.end();
}

int MWMechanics::NpcStats::getBounty() const
{
    return mBounty;
}

void MWMechanics::NpcStats::setBounty (int bounty)
{
    mBounty = bounty;
}

int MWMechanics::NpcStats::getFactionReputation (const std::string& faction) const
{
    std::map<std::string, int>::const_iterator iter = mFactionReputation.find (faction);

    if (iter==mFactionReputation.end())
        return 0;

    return iter->second;
}

void MWMechanics::NpcStats::setFactionReputation (const std::string& faction, int value)
{
    mFactionReputation[faction] = value;
}

int MWMechanics::NpcStats::getReputation() const
{
    return mReputation;
}

void MWMechanics::NpcStats::setReputation(int reputation)
{
    mReputation = reputation;
}

bool MWMechanics::NpcStats::hasSkillsForRank (const std::string& factionId, int rank) const
{
    if (rank<0 || rank>=10)
        throw std::runtime_error ("rank index out of range");

    const ESM::Faction& faction =
        *MWBase::Environment::get().getWorld()->getStore().get<ESM::Faction>().find (factionId);

    std::vector<int> skills;

    for (int i=0; i<6; ++i)
        skills.push_back (static_cast<int> (getSkill (faction.mData.mSkills[i]).getModified()));

    std::sort (skills.begin(), skills.end());

    std::vector<int>::const_reverse_iterator iter = skills.rbegin();

    const ESM::RankData& rankData = faction.mData.mRankData[rank];

    if (*iter<rankData.mSkill1)
        return false;

    return *++iter>=rankData.mSkill2;
}

bool MWMechanics::NpcStats::isWerewolf() const
{
    return mIsWerewolf;
}

void MWMechanics::NpcStats::setWerewolf (bool set)
{
    if(set != false)
    {
        const MWWorld::Store<ESM::GameSetting> &gmst = MWBase::Environment::get().getWorld()->getStore().get<ESM::GameSetting>();

        for(size_t i = 0;i < ESM::Attribute::Length;i++)
        {
            mWerewolfAttributes[i] = getAttribute(i);
            // Oh, Bethesda. It's "Intelligence".
            std::string name = "fWerewolf"+((i==ESM::Attribute::Intelligence) ? std::string("Intellegence") :
                                            ESM::Attribute::sAttributeNames[i]);
            mWerewolfAttributes[i].setBase(int(gmst.find(name)->getFloat()));
        }

        for(size_t i = 0;i < ESM::Skill::Length;i++)
        {
            mWerewolfSkill[i] = getSkill(i);

            // Acrobatics is set separately for some reason.
            if(i == ESM::Skill::Acrobatics)
                continue;

            // "Mercantile"! >_<
            std::string name = "fWerewolf"+((i==ESM::Skill::Mercantile) ? std::string("Merchantile") :
                                            ESM::Skill::sSkillNames[i]);
            mWerewolfSkill[i].setBase(int(gmst.find(name)->getFloat()));
        }
    }
    mIsWerewolf = set;
}

int MWMechanics::NpcStats::getWerewolfKills() const
{
    return mWerewolfKills;
}

int MWMechanics::NpcStats::getProfit() const
{
    return mProfit;
}

void MWMechanics::NpcStats::modifyProfit(int diff)
{
    mProfit += diff;
}

float MWMechanics::NpcStats::getTimeToStartDrowning() const
{
    return mTimeToStartDrowning;
}
void MWMechanics::NpcStats::setTimeToStartDrowning(float time)
{
    assert(time>=0 && time<=20);
    mTimeToStartDrowning=time;
}
