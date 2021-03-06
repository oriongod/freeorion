#include "ClientApp.h"

#include "../combat/CombatOrder.h"
#include "../util/MultiplayerCommon.h"
#include "../util/Serialize.h"
#include "../universe/UniverseObject.h"
#include "../Empire/Empire.h"
#include "../Empire/EmpireManager.h"
#include "../network/Networking.h"

#include <stdexcept>


// static member(s)
ClientApp* ClientApp::s_app = 0;

ClientApp::ClientApp() :
    m_universe(),
    m_empire_id(ALL_EMPIRES),
    m_current_turn(INVALID_GAME_TURN)
{
#ifdef FREEORION_BUILD_HUMAN
    EmpireEliminatedSignal.connect(boost::bind(&Universe::HandleEmpireElimination, &m_universe, _1));
#endif

    if (s_app)
        throw std::runtime_error("Attempted to construct a second instance of ClientApp");
    s_app = this;
}

ClientApp::~ClientApp()
{}

int ClientApp::PlayerID() const
{ return m_networking.PlayerID(); }

int ClientApp::EmpireID() const
{ return m_empire_id; }

int ClientApp::CurrentTurn() const
{ return m_current_turn; }

const Universe& ClientApp::GetUniverse() const
{ return m_universe; }

const EmpireManager& ClientApp::Empires() const
{ return m_empires; }

const OrderSet& ClientApp::Orders() const
{ return m_orders; }

const CombatOrderSet& ClientApp::CombatOrders() const
{ return m_combat_orders; }

const ClientNetworking& ClientApp::Networking() const
{ return m_networking; }

int ClientApp::EmpirePlayerID(int empire_id) const {
    for (std::map<int, PlayerInfo>::const_iterator it = m_player_info.begin(); it != m_player_info.end(); ++it)
        if (it->second.empire_id == empire_id)
            return it->first;
    return Networking::INVALID_PLAYER_ID;
}

const Networking::ClientType ClientApp::GetEmpireClientType(int empire_id) const
{ return GetPlayerClientType(ClientApp::EmpirePlayerID(empire_id)); }

const Networking::ClientType ClientApp::GetPlayerClientType(int player_id) const {
    if (player_id == Networking::INVALID_PLAYER_ID)
        return Networking::INVALID_CLIENT_TYPE;
    std::map<int, PlayerInfo>::const_iterator it = m_player_info.find(player_id);
    if (it != m_player_info.end())
        return it->second.client_type;
    return Networking::INVALID_CLIENT_TYPE;
}

const std::map<int, PlayerInfo>& ClientApp::Players() const
{ return m_player_info; }

std::map<int, PlayerInfo>& ClientApp::Players()
{ return m_player_info; }

void ClientApp::StartTurn() {
    m_networking.SendMessage(TurnOrdersMessage(m_networking.PlayerID(), m_orders));
    m_orders.Reset();
}

void ClientApp::SendCombatSetup() {
    m_networking.SendMessage(CombatTurnOrdersMessage(m_networking.PlayerID(), m_combat_orders));
    m_combat_orders.clear();
}

void ClientApp::StartCombatTurn() {
    m_networking.SendMessage(CombatTurnOrdersMessage(m_networking.PlayerID(), m_combat_orders));
    m_combat_orders.clear();
}

Universe& ClientApp::GetUniverse()
{ return m_universe; }

EmpireManager& ClientApp::Empires()
{ return m_empires; }

OrderSet& ClientApp::Orders()
{ return m_orders; }

CombatOrderSet& ClientApp::CombatOrders()
{ return m_combat_orders; }

ClientNetworking& ClientApp::Networking()
{ return m_networking; }

int ClientApp::GetNewObjectID() {
    Message msg;
    m_networking.SendSynchronousMessage(RequestNewObjectIDMessage(m_networking.PlayerID()), msg);
    std::string text = msg.Text();
    if (text.empty()) {
        throw std::runtime_error("ClientApp::GetNewObjectID() didn't get a new object ID");
    }
    return boost::lexical_cast<int>(text);
}

int ClientApp::GetNewDesignID() {
    Message msg;
    m_networking.SendSynchronousMessage(RequestNewDesignIDMessage(m_networking.PlayerID()), msg);
    std::string text = msg.Text();
    if (text.empty()) {
        throw std::runtime_error("ClientApp::GetNewDesignID() didn't get a new design ID");
    }
    return boost::lexical_cast<int>(text);
}

ClientApp* ClientApp::GetApp()
{ return s_app; }

void ClientApp::SetEmpireID(int id)
{ m_empire_id = id; }

void ClientApp::SetCurrentTurn(int turn)
{ m_current_turn = turn; }
