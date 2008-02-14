#include "PythonAI.h"
#include "../util/AppInterface.h"
#include "../util/Directories.h"

#include "../Empire/Empire.h"

#include "../universe/Universe.h"
#include "../universe/UniverseObject.h"
#include "../universe/Fleet.h"
#include "../universe/Ship.h"
#include "../universe/ShipDesign.h"
#include "../universe/Building.h"
#include "../universe/ResourceCenter.h"
#include "../universe/PopCenter.h"
#include "../universe/Planet.h"
#include "../universe/System.h"
#include "../universe/Special.h"

#include "../universe/Enums.h"

#include <boost/python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>
#include <boost/python/suite/indexing/map_indexing_suite.hpp>

#include <boost/lexical_cast.hpp>

using boost::python::class_;
using boost::python::bases;
using boost::python::def;
using boost::python::iterator;
using boost::python::no_init;
using boost::noncopyable;
using boost::python::return_value_policy;
using boost::python::copy_const_reference;
using boost::python::reference_existing_object;
using boost::python::return_by_value;
using boost::python::enum_;
using boost::python::vector_indexing_suite;
using boost::python::map_indexing_suite;
using boost::python::object;
using boost::python::import;
using boost::python::error_already_set;
using boost::python::exec;
using boost::python::dict;
using boost::python::extract;

namespace {
    //////////////////////////
    //    STL Containers    //
    //////////////////////////

    /* SetWrapper class encapsulates functions that expose the STL std::set<> class to Python in a limited,
       read-only fashion.  The set can be iterated through in Python, and printed. */
    template <typename ElementType>
    class SetWrapper {
    public:
        typedef typename std::set<ElementType> Set;
        typedef typename Set::const_iterator SetIterator;

        static unsigned int size(const Set& self) {
            return static_cast<unsigned int>(self.size());  // ignore warning http://lists.boost.org/Archives/boost/2007/04/120377.php
        }
        static bool empty(const Set& self) { return self.empty(); }
        static bool contains(const Set& self, const ElementType& item) { return self.find(item) != self.end(); }
        static unsigned int count(const Set& self, const ElementType& item) { return self.find(item) == self.end() ? 0u : 1u; }
        static SetIterator begin(const Set& self) { return self.begin(); }
        static SetIterator end(const Set& self) { return self.end(); }
        static std::string to_string(const Set& self) {
            std::string retval = "set([";
            for (SetIterator it = self.begin(); it != self.end(); ++it) {
                if (it != self.end() && it != self.begin()) {
                    retval += ", ";
                }
                try {
                    std::string s = boost::lexical_cast<std::string>(*it);
                    retval += s;
                } catch (...) {
                    retval += "?";
                }
            }
            retval += "])";
            return retval;
        };

        static void Wrap(const std::string& python_name) {
            class_<Set, noncopyable>(python_name.c_str(), no_init)
                .def("__str__",         &to_string)
                .def("__len__",         &size)
                .def("size",            &size)
                .def("empty",           &empty)
                .def("__contains__",    &contains)
                .def("count",           &count)
                .def("__iter__",        iterator<Set>())
                ;
        }
    };
}

////////////////////////
// Python AIInterface //
////////////////////////

// disambiguation of overloaded functions
const std::string&      (*AIIntPlayerNameVoid)(void) =          &AIInterface::PlayerName;
const std::string&      (*AIIntPlayerNameInt)(int) =            &AIInterface::PlayerName;

const Empire*           (*AIIntGetEmpireVoid)(void) =           &AIInterface::GetEmpire;
const Empire*           (*AIIntGetEmpireInt)(int) =             &AIInterface::GetEmpire;

const UniverseObject*   (Universe::*UniverseGetObject)(int) =   &Universe::Object;
const Fleet*            (Universe::*UniverseGetFleet)(int) =    &Universe::Object;
const Ship*             (Universe::*UniverseGetShip)(int) =     &Universe::Object;
const Planet*           (Universe::*UniverseGetPlanet)(int) =   &Universe::Object;
const System*           (Universe::*UniverseGetSystem)(int) =   &Universe::Object;
const Building*         (Universe::*UniverseGetBuilding)(int) = &Universe::Object;

int                     (*AIIntEnqueueBuildingOrbital)(BuildType, const std::string&, int) =
                                                                &AIInterface::IssueEnqueueProductionOrder;
int                     (*AIIntEnqueueShip)(BuildType, int, int) =
                                                                &AIInterface::IssueEnqueueProductionOrder;

namespace {
    // static s_save_state_string, getter and setter to be exposed to Python
    static std::string  s_save_state_string("");
    static const std::string& GetStaticSaveStateString() {
        //Logger().debugStream() << "Python-exposed GetSaveStateString() returning " << s_save_state_string;
        return s_save_state_string;
    }
    static void SetStaticSaveStateString(const std::string& new_state_string) {
        s_save_state_string = new_state_string;
        //Logger().debugStream() << "Python-exposed SetSaveStateString(" << s_save_state_string << ")";
    }
}

// Expose interface for redirecting standard output and error to FreeOrion logging.  Can be imported
// before loading the main FreeOrion AI interface library.
static const int MAX_SINGLE_CHUNK_TEXT_SIZE = 1000; 
static std::string log_buffer("");
void LogText(const char* text) {
    // Python sends text as several null-terminated array of char which need to be
    // concatenated before they are output to the logger.  There's probably a better
    // way to do this, but I don't know what it is, and this seems reasonably safe...
    if (!text) return;
    for (int i = 0; i < MAX_SINGLE_CHUNK_TEXT_SIZE; ++i) {
        if (text[i] == '\0') break;
        if (text[i] == '\n' || i == MAX_SINGLE_CHUNK_TEXT_SIZE - 1) {
            AIInterface::LogOutput(log_buffer);
            log_buffer = "";
        } else {
            log_buffer += text[i];
        }
    }
}

static std::string error_buffer("");
void ErrorText(const char* text) {
    // Python sends text as several null-terminated array of char which need to be
    // concatenated before they are output to the logger.  There's probably a better
    // way to do this, but I don't know what it is, and this seems reasonably safe...
   if (!text) return;
    for (int i = 0; i < MAX_SINGLE_CHUNK_TEXT_SIZE; ++i) {
        if (text[i] == '\0') break;
        if (text[i] == '\n' || i == MAX_SINGLE_CHUNK_TEXT_SIZE - 1) {
            AIInterface::ErrorOutput(error_buffer);
            error_buffer = "";
        } else {
            error_buffer += text[i];
        }
    }
}

// Expose minimal debug and error (stdout and stderr respectively) sinks so Python text output can be
// recovered and saved in c++
BOOST_PYTHON_MODULE(freeOrionLogger)
{
    def("log",                    LogText);
    def("error",                  ErrorText);
}

// Expose AIInterface and all associated classes to Python
BOOST_PYTHON_MODULE(freeOrionAIInterface)
{
    ///////////////////
    //  AIInterface  //
    ///////////////////
    def("playerName",               AIIntPlayerNameVoid,            return_value_policy<copy_const_reference>());
    def("playerName",               AIIntPlayerNameInt,             return_value_policy<copy_const_reference>());

    def("playerID",                 AIInterface::PlayerID);
    def("empirePlayerID",           AIInterface::EmpirePlayerID);
    def("allPlayerIDs",             AIInterface::AllPlayerIDs,      return_value_policy<return_by_value>());

    def("playerIsAI",               AIInterface::PlayerIsAI);
    def("playerIsHost",             AIInterface::PlayerIsHost);

    def("empireID",                 AIInterface::EmpireID);
    def("playerEmpireID",           AIInterface::PlayerEmpireID);
    def("allEmpireIDs",             AIInterface::AllEmpireIDs,      return_value_policy<return_by_value>());

    def("getEmpire",                AIIntGetEmpireVoid,             return_value_policy<reference_existing_object>());
    def("getEmpire",                AIIntGetEmpireInt,              return_value_policy<reference_existing_object>());

    def("getUniverse",              AIInterface::GetUniverse,       return_value_policy<reference_existing_object>());

    def("currentTurn",              AIInterface::CurrentTurn);

    def("issueFleetMoveOrder",          AIInterface::IssueFleetMoveOrder);
    def("issueRenameOrder",             AIInterface::IssueRenameOrder);
    def("issueNewFleetOrder",           AIInterface::IssueNewFleetOrder);
    def("issueColonizeOrder",           AIInterface::IssueFleetColonizeOrder);
    def("issueChangeFocusOrder",        AIInterface::IssueChangeFocusOrder);
    def("issueEnqueueTechOrder",        AIInterface::IssueEnqueueTechOrder);
    def("issueDequeueTechOrder",        AIInterface::IssueDequeueTechOrder);
    def("issueEnqueueProductionOrder",  AIIntEnqueueBuildingOrbital);
    def("issueEnqueueProductionOrder",  AIIntEnqueueShip);
    def("issueRequeueProductionOrder",  AIInterface::IssueRequeueProductionOrder);
    def("issueDequeueProductionOrder",  AIInterface::IssueDequeueProductionOrder);

    def("sendChatMessage",          AIInterface::SendPlayerChatMessage);

    def("setSaveStateString",       SetStaticSaveStateString);
    def("getSaveStateString",       GetStaticSaveStateString,               return_value_policy<copy_const_reference>());

    def("doneTurn",                 AIInterface::DoneTurn);


    ///////////////////
    //     Empire    //
    ///////////////////
    class_<Empire, noncopyable>("empire", no_init)
        .add_property("name",           make_function(&Empire::Name,        return_value_policy<copy_const_reference>()))
        .add_property("playerName",     make_function(&Empire::PlayerName,  return_value_policy<copy_const_reference>()))

        .add_property("empireID",       &Empire::EmpireID)
        .add_property("homeworldID",    &Empire::HomeworldID)
        .add_property("capitolID",      &Empire::CapitolID)

        .def("buildingTypeAvailable",   &Empire::BuildingTypeAvailable)
        .def("availableBuildingTypes",  &Empire::AvailableBuildingTypes,    return_value_policy<copy_const_reference>()) 
        .def("techResearched",          &Empire::TechResearched)
        .def("availableTechs",          &Empire::AvailableTechs,            return_value_policy<copy_const_reference>()) 
        .def("getTechStatus",           &Empire::GetTechStatus)
        .def("researchStatus",          &Empire::ResearchStatus)

        .def("hasExploredSystem",       &Empire::HasExploredSystem)
    ;

    ////////////////////
    //    Universe    //
    ////////////////////
    class_<Universe, noncopyable>("universe", no_init)
        .def("getObject",                   UniverseGetObject,              return_value_policy<reference_existing_object>())
        .def("getFleet",                    UniverseGetFleet,               return_value_policy<reference_existing_object>())
        .def("getShip",                     UniverseGetShip,                return_value_policy<reference_existing_object>())
        .def("getPlanet",                   UniverseGetPlanet,              return_value_policy<reference_existing_object>())
        .def("getSystem",                   UniverseGetSystem,              return_value_policy<reference_existing_object>())
        .def("getBuilding",                 UniverseGetBuilding,            return_value_policy<reference_existing_object>())
        .def("getSpecial",                  GetSpecial,                     return_value_policy<reference_existing_object>())

        .add_property("allObjectIDs",       make_function(&Universe::FindObjectIDs<UniverseObject>, return_value_policy<return_by_value>()))
        // TODO: add ability to iterate over all objects in universe

        .def("systemHasStarlane",           &Universe::SystemReachable)
        .def("systemsConnected",            &Universe::SystemsConnected)

        // put as part of universe class so one doesn't need a UniverseObject object in python to access these
        .def_readonly("invalidObjectID",    &UniverseObject::INVALID_OBJECT_ID)
        .def_readonly("invalidObjectAge",   &UniverseObject::INVALID_OBJECT_AGE)
    ;

    ////////////////////
    // UniverseObject //
    ////////////////////
    class_<UniverseObject, noncopyable>("universeObject", no_init)
        .add_property("id",                 &UniverseObject::ID)
        .add_property("name",               make_function(&UniverseObject::Name,        return_value_policy<copy_const_reference>()))
        .add_property("x",                  &UniverseObject::X)
        .add_property("y",                  &UniverseObject::Y)
        .add_property("systemID",           &UniverseObject::SystemID)
        .add_property("unowned",            &UniverseObject::Unowned)
        .add_property("owners",             make_function(&UniverseObject::Owners,      return_value_policy<reference_existing_object>()))
        .def("ownedBy",                     &UniverseObject::OwnedBy)
        .def("whollyOwnedBy",               &UniverseObject::WhollyOwnedBy)
        .add_property("creationTurn",       &UniverseObject::CreationTurn)
        .add_property("ageInTurns",         &UniverseObject::AgeInTurns)
        .add_property("specials",           make_function(&UniverseObject::Specials,    return_value_policy<reference_existing_object>()))
    ;

    ///////////////////
    //     Fleet     //
    ///////////////////
    class_<Fleet, bases<UniverseObject>, noncopyable>("fleet", no_init)
        .add_property("finalDestinationID",         &Fleet::FinalDestinationID)
        .add_property("nextSystemID",               &Fleet::NextSystemID)
        .add_property("speed",                      &Fleet::Speed)
        .add_property("canChangeDirectionEnRoute",  &Fleet::CanChangeDirectionEnRoute)
        .add_property("hasArmedShips",              &Fleet::HasArmedShips)
        .add_property("numShips",                   &Fleet::NumShips)
        .def("containsShipID",                      &Fleet::ContainsShip)
        .add_property("shipIDs",                    make_function(&Fleet::ShipIDs,      return_value_policy<reference_existing_object>()))
        // TODO: add ability to iterate over ships in fleet
    ;

    //////////////////
    //     Ship     //
    //////////////////
    class_<Ship, bases<UniverseObject>, noncopyable>("ship", no_init)
        .add_property("design",             make_function(&Ship::Design,        return_value_policy<reference_existing_object>()))
        .add_property("fleetID",            &Ship::FleetID)
        .add_property("getFleet",           make_function(&Ship::GetFleet,      return_value_policy<reference_existing_object>()))
        .add_property("isArmed",            &Ship::IsArmed)
        .add_property("speed",              &Ship::Speed)
    ;

    //////////////////
    //  ShipDesign  //
    //////////////////
    class_<ShipDesign, noncopyable>("shipDesign", no_init)
        .add_property("name",               make_function(&ShipDesign::Name,    return_value_policy<copy_const_reference>()))
    ;

    //////////////////
    //   Building   //
    //////////////////
    class_<Building, bases<UniverseObject>, noncopyable>("building", no_init)
        .def("getBuildingType",             &Building::GetBuildingType,         return_value_policy<reference_existing_object>())
        .add_property("operating",          &Building::Operating)
        .def("getPlanet",                   &Building::GetPlanet,               return_value_policy<reference_existing_object>())
    ;

    //////////////////
    // BuildingType //
    //////////////////
    class_<BuildingType, noncopyable>("buildingType", no_init)
        .add_property("name",               make_function(&BuildingType::Name,          return_value_policy<copy_const_reference>()))
        .add_property("description",        make_function(&BuildingType::Description,   return_value_policy<copy_const_reference>()))
        .add_property("buildCost",          &BuildingType::BuildCost)
        .add_property("buildTime",          &BuildingType::BuildTime)
        .add_property("maintenanceCost",    &BuildingType::MaintenanceCost)
        .def("captureResult",               &BuildingType::GetCaptureResult)
    ;

    ////////////////////
    // ResourceCenter //
    ////////////////////
    class_<ResourceCenter, noncopyable>("resourceCenter", no_init)
        .add_property("primaryFocus",       &ResourceCenter::PrimaryFocus)
        .add_property("secondaryFocus",     &ResourceCenter::SecondaryFocus)
    ;

    ///////////////////
    //   PopCenter   //
    ///////////////////
    class_<PopCenter, noncopyable>("popCenter", no_init)
        .add_property("inhabitants",        &PopCenter::Inhabitants)
        .add_property("availableFood",      &PopCenter::AvailableFood)
    ;

    //////////////////
    //    Planet    //
    //////////////////
    class_<Planet, bases<UniverseObject, PopCenter, ResourceCenter>, noncopyable>("planet", no_init)
        .add_property("size",               &Planet::Size)
        .add_property("type",               &Planet::Type)
        .add_property("buildings",          make_function(&Planet::Buildings,   return_value_policy<reference_existing_object>()))
        // TODO: add ability to iterate over buildings on planet
    ;

    //////////////////
    //    System    //
    //////////////////
    class_<System, bases<UniverseObject>, noncopyable>("system", no_init)
        .add_property("starType",           &System::Star)
        .add_property("numOrbits",          &System::Orbits)
        .add_property("numStarlanes",       &System::Starlanes)
        .add_property("numWormholes",       &System::Wormholes)
        .def("HasStarlaneToSystemID",       &System::HasStarlaneTo)
        .def("HasWormholeToSystemID",       &System::HasWormholeTo)
        // TODO: add ability to iterate over planets in system
    ;

    //////////////////
    //     Tech     //
    //////////////////
    class_<Tech, noncopyable>("tech", no_init)
        .add_property("name",               make_function(&Tech::Name,              return_value_policy<copy_const_reference>()))
        .add_property("description",        make_function(&Tech::Description,       return_value_policy<copy_const_reference>()))
        .add_property("shortDescription",   make_function(&Tech::ShortDescription,  return_value_policy<copy_const_reference>()))
        .add_property("type",               &Tech::Type)
        .add_property("category",           make_function(&Tech::Category,          return_value_policy<copy_const_reference>()))
        .add_property("researchCost",       &Tech::ResearchCost)
        .add_property("researchTurns",      &Tech::ResearchTurns)
        .add_property("prerequisites",      make_function(&Tech::Prerequisites,     return_value_policy<reference_existing_object>()))
        .add_property("unlockedTechs",      make_function(&Tech::UnlockedTechs,     return_value_policy<reference_existing_object>()))
    ;

    /////////////////
    //   Special   //
    /////////////////
    class_<Special, noncopyable>("special", no_init)
        .add_property("name",               make_function(&Special::Name,           return_value_policy<copy_const_reference>()))
        .add_property("description",        make_function(&Special::Description,    return_value_policy<copy_const_reference>()))
    ;


    ////////////////////
    //     Enums      //
    ////////////////////
    enum_<StarType>("starType")
        .value("blue",      STAR_BLUE)
        .value("white",     STAR_WHITE)
        .value("yellow",    STAR_YELLOW)
        .value("orange",    STAR_ORANGE)
        .value("red",       STAR_RED)
        .value("neutron",   STAR_NEUTRON)
        .value("blackHole", STAR_BLACK)
    ;
    enum_<PlanetSize>("planetSize")
        .value("tiny",      SZ_TINY)
        .value("small",     SZ_SMALL)
        .value("medium",    SZ_MEDIUM)
        .value("large",     SZ_LARGE)
        .value("huge",      SZ_HUGE)
        .value("asteroids", SZ_ASTEROIDS)
        .value("gasGiant",  SZ_GASGIANT)
    ;
    enum_<PlanetType>("planetType")
        .value("swamp",     PT_SWAMP)
        .value("radiated",  PT_RADIATED)
        .value("toxic",     PT_TOXIC)
        .value("inferno",   PT_INFERNO)
        .value("barren",    PT_BARREN)
        .value("tundra",    PT_TUNDRA)
        .value("desert",    PT_DESERT)
        .value("terran",    PT_TERRAN)
        .value("ocean",     PT_OCEAN)
        .value("asteroids", PT_ASTEROIDS)
        .value("gasGiant",  PT_GASGIANT)
    ;
    enum_<PlanetEnvironment>("planetEnvironment")
        .value("uninhabitable", PE_UNINHABITABLE)
        .value("hostile",       PE_HOSTILE)
        .value("poor",          PE_POOR)
        .value("adequate",      PE_ADEQUATE)
        .value("good",          PE_GOOD)
    ;
    enum_<TechType>("techType")
        .value("theory",        TT_THEORY)
        .value("application",   TT_APPLICATION)
        .value("refinement",    TT_REFINEMENT)
    ;
    enum_<TechStatus>("techStatus")
        .value("unresearchable",    TS_UNRESEARCHABLE)
        .value("researchable",      TS_RESEARCHABLE)
        .value("complete",          TS_COMPLETE)
    ;
    enum_<MeterType>("meterType")
        .value("population",    METER_POPULATION)
        .value("farming",       METER_FARMING)
        .value("industry",      METER_INDUSTRY)
        .value("research",      METER_RESEARCH)
        .value("trade",         METER_TRADE)
        .value("mining",        METER_MINING)
        .value("construction",  METER_CONSTRUCTION)
        .value("health",        METER_HEALTH)
        .value("fuel",          METER_FUEL)
        .value("supply",        METER_SUPPLY)
        .value("stealth",       METER_STEALTH)
        .value("detection",     METER_DETECTION)
        .value("shield",        METER_SHIELD)
        .value("defense",       METER_DEFENSE)
    ;
    enum_<FocusType>("focusType")
        .value("balanced",  FOCUS_BALANCED)
        .value("farming",   FOCUS_FARMING)
        .value("industry",  FOCUS_INDUSTRY)
        .value("mining",    FOCUS_MINING)
        .value("research",  FOCUS_RESEARCH)
        .value("trade",     FOCUS_TRADE)
    ;
    enum_<CaptureResult>("captureResult")
        .value("capture",   CR_CAPTURE)
        .value("destroy",   CR_DESTROY)
        .value("retain",    CR_RETAIN)
        .value("share",     CR_SHARE)
    ;


    ////////////////////
    // STL Containers //
    ////////////////////
    class_<std::vector<int> >("IntVec")
        .def(vector_indexing_suite<std::vector<int> >())
    ;
    class_<std::vector<std::string> >("StringVec")
        .def(vector_indexing_suite<std::vector<std::string> >())
    ;

    SetWrapper<int>::Wrap("IntSet");
    SetWrapper<std::string>::Wrap("StringSet");
}
 
///////////////////////
//     PythonAI      //
///////////////////////
static dict         s_main_namespace = dict();
static object       s_ai_module = object();
static PythonAI*    s_ai = 0;
PythonAI::PythonAI() {
    // in order to expose a getter for it to Python, s_save_state_string must be static, and not a member
    // variable of class PythonAI, because the exposing is done outside the PythonAI class and there is no
    // access to a pointer to PythonAI
    if (s_ai)
        throw std::runtime_error("Attempted to create more than one Python AI instance");

    s_ai = this;

    Py_Initialize();    // initializes Python interpreter, allowing Python functions to be called from C++

    initfreeOrionLogger();          // allows the "freeOrionLogger" C++ module to be imported within Python code
    initfreeOrionAIInterface();     // allows the "freeOrionAIInterface" C++ module to be imported within Python code

    try {
        // get main namespace, needed to run other interpreted code
        object main_module = import("__main__");
        s_main_namespace = extract<dict>(main_module.attr("__dict__"));
    } catch (error_already_set err) {
        Logger().errorStream() << "Unable to initialize Python interpreter.";
        return;
    }

    try {
        // set up Logging by redirecting stdout and stderr to exposed logging functions
        std::string logger_script = "import sys\n"
                                    "import freeOrionLogger\n"
                                    "class debugLogger:\n"
                                    "  def write(self, stng):\n"
                                    "    freeOrionLogger.log(stng)\n"
                                    "class errorLogger:\n"
                                    "  def write(self, stng):\n"
                                    "    freeOrionLogger.error(stng)\n"
                                    "sys.stdout = debugLogger()\n"
                                    "sys.stderr = errorLogger()\n"
                                    "print 'Python stdout and stderr redirected'";
        object ignored = exec(logger_script.c_str(), s_main_namespace, s_main_namespace);
    } catch (error_already_set err) {
        Logger().errorStream() << "Unable to redirect Python stdout and stderr.";
        return;
    }

    try {
        // tell Python the path in which to locate AI script file
        std::string AI_path = (GetGlobalDir() / "default" / "AI").native_directory_string();
        std::string path_command = "sys.path.append('" + AI_path + "')";
        object ignored = exec(path_command.c_str(), s_main_namespace, s_main_namespace);

        // import AI script file and run initialization function
        s_ai_module = import("FreeOrionAI");
        object initAIPythonFunction = s_ai_module.attr("initFreeOrionAI");
        initAIPythonFunction();

        //ignored = exec(fo_interface_import_script.c_str(), s_main_namespace, s_main_namespace);
    } catch (error_already_set err) {
        PyErr_Print();
        return;
    }

    Logger().debugStream() << "Initialized Python AI";
}

PythonAI::~PythonAI() {
    Logger().debugStream() << "Cleaning up / destructing Python AI";
    Py_Finalize();      // stops Python interpreter and release its resources
    s_ai = 0;
}

void PythonAI::GenerateOrders() {
    try {
        // call Python function that generates orders for current turn
        object generateOrdersPythonFunction = s_ai_module.attr("generateOrders");
        generateOrdersPythonFunction();
    } catch (error_already_set err) {
        PyErr_Print();
        AIInterface::DoneTurn();
    }
}

void PythonAI::HandleChatMessage(int sender_id, const std::string& msg) {
    try {
        // call Python function that responds or ignores a chat message
        object handleChatMessagePythonFunction = s_ai_module.attr("handleChatMessage");
        handleChatMessagePythonFunction(sender_id, msg);
    } catch (error_already_set err) {
        PyErr_Print();
    }
}

void PythonAI::StartNewGame() {
    s_save_state_string = "";
    try {
        // call Python function that sets up the AI to be able to generate orders for a new game
        object startNewGamePythonFunction = s_ai_module.attr("startNewGame");
        startNewGamePythonFunction();
    } catch (error_already_set err) {
        PyErr_Print();
    }
}

void PythonAI::ResumeLoadedGame(const std::string& save_state_string) {
    Logger().debugStream() << "PythonAI::ResumeLoadedGame(" << save_state_string << ")";
    s_save_state_string = save_state_string;
    try {
        // call Python function that deals with the new state string sent by the server
        object resumeLoadedGamePythonFunction = s_ai_module.attr("resumeLoadedGame");
        resumeLoadedGamePythonFunction(s_save_state_string);
    } catch (error_already_set err) {
        PyErr_Print();
    }
}

const std::string& PythonAI::GetSaveStateString() {
    try {
        // call Python function that serializes AI state for storage in save file and sets s_save_state_string
        // to contain that string
        object prepareForSavePythonFunction = s_ai_module.attr("prepareForSave");
        prepareForSavePythonFunction();
    } catch (error_already_set err) {
        PyErr_Print();
    }
    Logger().debugStream() << "PythonAI::GetSaveStateString() returning: " << s_save_state_string;
    return s_save_state_string;
}
