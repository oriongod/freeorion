#include "Species.h"

#include "Effect.h"
#include "Condition.h"
#include "../parse/Parse.h"
#include "../util/MultiplayerCommon.h"
#include "../util/OptionsDB.h"
#include "../util/Directories.h"
#include "../util/Random.h"

#include <boost/filesystem/fstream.hpp>

std::string DumpIndent();

extern int g_indent;


/////////////////////////////////////////////////
// FocusType                                   //
/////////////////////////////////////////////////
std::string FocusType::Dump() const {
    std::string retval = DumpIndent() + "FocusType\n";
    ++g_indent;
    retval += DumpIndent() + "name = \"" + m_name + "\"\n";
    retval += DumpIndent() + "description = \"" + m_description + "\"\n";
    retval += DumpIndent() + "location = \n";
    ++g_indent;
    retval += m_location->Dump();
    --g_indent;
    retval += DumpIndent() + "graphic = \"" + m_graphic + "\"\n";
    --g_indent;
    return retval;
}

/////////////////////////////////////////////////
// Species                                     //
/////////////////////////////////////////////////
namespace {
    std::string PlanetTypeToString(PlanetType type) {
        switch (type) {
        case PT_SWAMP:      return "Swamp";
        case PT_TOXIC:      return "Toxic";
        case PT_INFERNO:    return "Inferno";
        case PT_RADIATED:   return "Radiated";
        case PT_BARREN:     return "Barren";
        case PT_TUNDRA:     return "Tundra";
        case PT_DESERT:     return "Desert";
        case PT_TERRAN:     return "Terran";
        case PT_OCEAN:      return "Ocean";
        case PT_ASTEROIDS:  return "Asteroids";
        case PT_GASGIANT:   return "GasGiant";
        default:            return "?";
        }
    }
    std::string PlanetEnvironmentToString(PlanetEnvironment env) {
        switch (env) {
        case PE_UNINHABITABLE:  return "Uninhabitable";
        case PE_HOSTILE:        return "Hostile";
        case PE_POOR:           return "Poor";
        case PE_ADEQUATE:       return "Adequate";
        case PE_GOOD:           return "Good";
        default:                return "?";
        }
    }
}

std::string Species::Dump() const {
    std::string retval = DumpIndent() + "Species\n";
    ++g_indent;
    retval += DumpIndent() + "name = \"" + m_name + "\"\n";
    retval += DumpIndent() + "description = \"" + m_description + "\"\n";
    if (m_playable)
        retval += DumpIndent() + "Playable\n";
    if (m_native)
        retval += DumpIndent() + "Native\n";
    if (m_can_produce_ships)
        retval += DumpIndent() + "CanProduceShips\n";
    if (m_can_colonize)
        retval += DumpIndent() + "CanColonize\n";
    if (m_foci.size() == 1) {
        retval += DumpIndent() + "foci =\n";
        m_foci.begin()->Dump();
    } else {
        retval += DumpIndent() + "foci = [\n";
        ++g_indent;
        for (std::vector<FocusType>::const_iterator it = m_foci.begin(); it != m_foci.end(); ++it) {
            retval += it->Dump();
        }
        --g_indent;
        retval += DumpIndent() + "]\n";
    }
    if (m_effects.size() == 1) {
        retval += DumpIndent() + "effectsgroups =\n";
        ++g_indent;
        retval += m_effects[0]->Dump();
        --g_indent;
    } else {
        retval += DumpIndent() + "effectsgroups = [\n";
        ++g_indent;
        for (unsigned int i = 0; i < m_effects.size(); ++i) {
            retval += m_effects[i]->Dump();
        }
        --g_indent;
        retval += DumpIndent() + "]\n";
    }
    if (m_planet_environments.size() == 1) {
        retval += DumpIndent() + "environments =\n";
        ++g_indent;
        retval += DumpIndent() + "type = " + PlanetTypeToString(m_planet_environments.begin()->first)
                               + " environment = " + PlanetEnvironmentToString(m_planet_environments.begin()->second)
                               + "\n";
        --g_indent;
    } else {
        retval += DumpIndent() + "environments = [\n";
        ++g_indent;
        for (std::map<PlanetType, PlanetEnvironment>::const_iterator it = m_planet_environments.begin(); it != m_planet_environments.end(); ++it) {
            retval += DumpIndent() + "type = " + PlanetTypeToString(it->first)
                                   + " environment = " + PlanetEnvironmentToString(it->second)
                                   + "\n";
        }
        --g_indent;
        retval += DumpIndent() + "]\n";
    }
    retval += DumpIndent() + "graphic = \"" + m_graphic + "\"\n";
    --g_indent;
    return retval;
}

PlanetEnvironment Species::GetPlanetEnvironment(PlanetType planet_type) const {
    std::map<PlanetType, PlanetEnvironment>::const_iterator it = m_planet_environments.find(planet_type);
    if (it == m_planet_environments.end())
        return PE_UNINHABITABLE;
    else
        return it->second;
}

namespace {
    PlanetType RingNextPlanetType(PlanetType current_type) {
        PlanetType next(PlanetType(int(current_type)+1));
        if (next >= PT_ASTEROIDS)
            next = PT_SWAMP;
        return next;
    }
    PlanetType RingPreviousPlanetType(PlanetType current_type) {
        PlanetType next(PlanetType(int(current_type)-1));
        if (next <= INVALID_PLANET_TYPE)
            next = PT_OCEAN;
        return next;
    }
}

PlanetType Species::NextBetterPlanetType(PlanetType initial_planet_type) const
{
    // some types can't be terraformed
    if (initial_planet_type == PT_GASGIANT)
        return PT_GASGIANT;
    if (initial_planet_type == PT_ASTEROIDS)
        return PT_ASTEROIDS;
    if (initial_planet_type == INVALID_PLANET_TYPE)
        return INVALID_PLANET_TYPE;
    if (initial_planet_type == NUM_PLANET_TYPES)
        return NUM_PLANET_TYPES;
    // and sometimes there's no variation data
    if (m_planet_environments.empty())
        return initial_planet_type;

    // determine which environment rating is the best available for this species
    PlanetEnvironment best_environment = PE_UNINHABITABLE;
    //std::set<PlanetType> best_types;
    for (std::map<PlanetType, PlanetEnvironment>::const_iterator it = m_planet_environments.begin();
         it != m_planet_environments.end(); ++it)
    {
        if (it->second == best_environment) {
            //best_types.insert(it->first);
        } else if (it->second > best_environment) {
            best_environment = it->second;
            //best_types.clear();
            //best_types.insert(it->first);
        }
    }

    // if no improvement available, abort early
    PlanetEnvironment initial_environment = GetPlanetEnvironment(initial_planet_type);
    if (initial_environment >= best_environment)
        return initial_planet_type;

    // find which of the best types is closest to the current type
    int forward_steps_to_best = 0;
    for (PlanetType type = RingNextPlanetType(initial_planet_type); type != initial_planet_type; type = RingNextPlanetType(type)) {
        forward_steps_to_best++;
        if (GetPlanetEnvironment(type) == best_environment)
            break;
    }
    int backward_steps_to_best = 0;
    for (PlanetType type = RingPreviousPlanetType(initial_planet_type); type != initial_planet_type; type = RingPreviousPlanetType(type)) {
        backward_steps_to_best++;
        if (GetPlanetEnvironment(type) == best_environment)
            break;
    }
    if (forward_steps_to_best <= backward_steps_to_best)
        return RingNextPlanetType(initial_planet_type);
    else
        return RingPreviousPlanetType(initial_planet_type);
}

void Species::AddHomeworld(int homeworld_id) {
    if (!GetUniverseObject(homeworld_id))
        Logger().debugStream() << "Species asked to add homeworld id " << homeworld_id << " but there is no such object in the Universe";
    if (m_homeworlds.find(homeworld_id) != m_homeworlds.end())
        return;
    m_homeworlds.insert(homeworld_id);
    // TODO if needed: StateChangedSignal();
}

void Species::RemoveHomeworld(int homeworld_id) {
    if (m_homeworlds.find(homeworld_id) == m_homeworlds.end()) {
        Logger().debugStream() << "Species asked to remove homeworld id " << homeworld_id << " but doesn't have that id as a homeworld";
        return;
    }
    m_homeworlds.erase(homeworld_id);
    // TODO if needed: StateChangedSignal();
}

void Species::SetHomeworlds(const std::set<int>& homeworld_ids) {
    if (m_homeworlds == homeworld_ids)
        return;
    m_homeworlds = homeworld_ids;
    // TODO if needed: StateChangedSignal();
}


/////////////////////////////////////////////////
// SpeciesManager                              //
/////////////////////////////////////////////////
// static(s)
SpeciesManager* SpeciesManager::s_instance = 0;

bool SpeciesManager::PlayableSpecies::operator()(
    const std::map<std::string, Species*>::value_type& species_map_iterator) const
{ return species_map_iterator.second->Playable(); }

bool SpeciesManager::NativeSpecies::operator()(
    const std::map<std::string, Species*>::value_type& species_map_iterator) const
{ return species_map_iterator.second->Native(); }

SpeciesManager::SpeciesManager() {
    if (s_instance)
        throw std::runtime_error("Attempted to create more than one SpeciesManager.");
    s_instance = this;
    parse::species(GetResourceDir() / "species.txt", m_species);
    if (GetOptionsDB().Get<bool>("verbose-logging")) {
        Logger().debugStream() << "Species:";
        for (iterator it = begin(); it != end(); ++it) {
            const Species* s = it->second;
            Logger().debugStream() << " ... " << s->Name() << "  \t" <<
                (s->Playable() ?        "Playable " : "         ") <<
                (s->Native() ?          "Native " : "       ") <<
                (s->CanProduceShips() ? "CanProduceShips " : "                ") <<
                (s->CanColonize() ?     "CanColonize " : "            ");
        }
    }
}

SpeciesManager::~SpeciesManager() {
    for (std::map<std::string, Species*>::iterator it = m_species.begin(); it != m_species.end(); ++it)
        delete it->second;
}

const Species* SpeciesManager::GetSpecies(const std::string& name) const {
    std::map<std::string, Species*>::const_iterator it = m_species.find(name);
    return it != m_species.end() ? it->second : 0;
}

Species* SpeciesManager::GetSpecies(const std::string& name) {
    std::map<std::string, Species*>::iterator it = m_species.find(name);
    return it != m_species.end() ? it->second : 0;
}

int SpeciesManager::GetSpeciesID(const std::string& name) const {
    iterator it = m_species.find(name);
    if (it == m_species.end())
        return -1;
    return std::distance(m_species.begin(), it);
}

SpeciesManager& SpeciesManager::GetSpeciesManager() {
    static SpeciesManager manager;
    return manager;
}

SpeciesManager::iterator SpeciesManager::begin() const
{ return m_species.begin(); }

SpeciesManager::iterator SpeciesManager::end() const
{ return m_species.end(); }

SpeciesManager::playable_iterator SpeciesManager::playable_begin() const
{ return playable_iterator(PlayableSpecies(), m_species.begin(), m_species.end()); }

SpeciesManager::playable_iterator SpeciesManager::playable_end() const
{ return playable_iterator(PlayableSpecies(), m_species.end(), m_species.end()); }

SpeciesManager::native_iterator SpeciesManager::native_begin() const
{ return native_iterator(NativeSpecies(), m_species.begin(), m_species.end()); }

SpeciesManager::native_iterator SpeciesManager::native_end() const
{ return native_iterator(NativeSpecies(), m_species.end(), m_species.end()); }

bool SpeciesManager::empty() const
{ return m_species.empty(); }

int SpeciesManager::NumSpecies() const
{ return m_species.size(); }

int SpeciesManager::NumPlayableSpecies() const
{ return std::distance(playable_begin(), playable_end()); }

int SpeciesManager::NumNativeSpecies() const
{ return std::distance(native_begin(), native_end()); }

namespace {
    const std::string EMPTY_STRING;
}

const std::string& SpeciesManager::RandomSpeciesName() const {
    if (m_species.empty())
        return EMPTY_STRING;

    int species_idx = RandSmallInt(0, static_cast<int>(m_species.size()) - 1);
    std::map<std::string, Species*>::const_iterator it = m_species.begin();
    std::advance(it, species_idx);
    return it->first;
}

const std::string& SpeciesManager::RandomPlayableSpeciesName() const {
    if (NumPlayableSpecies() <= 0)
        return EMPTY_STRING;

    int species_idx = RandSmallInt(0, NumPlayableSpecies() - 1);
    playable_iterator it = playable_begin();
    std::advance(it, species_idx);
    return it->first;
}

void SpeciesManager::ClearSpeciesHomeworlds() {
    for (std::map<std::string, Species*>::iterator it = m_species.begin(); it != m_species.end(); ++it)
        it->second->SetHomeworlds(std::set<int>());
}

void SpeciesManager::SetSpeciesHomeworlds(const std::map<std::string, std::set<int> >& species_homeworld_ids) {
    ClearSpeciesHomeworlds();
    for (std::map<std::string, std::set<int> >::const_iterator it = species_homeworld_ids.begin(); it != species_homeworld_ids.end(); ++it) {
        const std::string& species_name = it->first;
        const std::set<int>& homeworlds = it->second;

        Species* species = 0;
        std::map<std::string, Species*>::iterator species_it = m_species.find(species_name);
        if (species_it != m_species.end())
            species = species_it->second;

        if (species) {
            species->SetHomeworlds(homeworlds);
        } else {
            Logger().errorStream() << "SpeciesManager::SetSpeciesHomeworlds couldn't find a species with name " << species_name << " to assign homeworlds to";
        }
    }
}

std::map<std::string, std::set<int> > SpeciesManager::GetSpeciesHomeworldsMap(int encoding_empire/* = ALL_EMPIRES*/) const {
    std::map<std::string, std::set<int> > retval;
    for (iterator it = begin(); it != end(); ++it) {
        const std::string species_name = it->first;
        const Species* species = it->second;
        if (!species) {
            Logger().errorStream() << "SpeciesManager::GetSpeciesHomeworldsMap found a null species pointer in SpeciesManager?!";
            continue;
        }
        const std::set<int>& homeworld_ids = species->Homeworlds();
        for (std::set<int>::const_iterator homeworlds_it = homeworld_ids.begin(); homeworlds_it != homeworld_ids.end(); ++homeworlds_it)
            retval[species_name].insert(*homeworlds_it);
    }
    return retval;
}


///////////////////////////////////////////////////////////
// Free Functions                                        //
///////////////////////////////////////////////////////////
SpeciesManager& GetSpeciesManager()
{ return SpeciesManager::GetSpeciesManager(); }

const Species* GetSpecies(const std::string& name)
{ return GetSpeciesManager().GetSpecies(name); }
