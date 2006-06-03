//SystemIcon.cpp

#include "SystemIcon.h"

#include "ClientUI.h"
#include "../universe/Fleet.h"
#include "FleetButton.h"
#include "FleetWnd.h"
#include "../client/human/HumanClientApp.h"
#include "MapWnd.h"
#include "../util/MultiplayerCommon.h"
#include "../universe/Predicates.h"
#include "../universe/System.h"
#include "../Empire/Empire.h"

#include <GG/DrawUtil.h>
#include <GG/StaticGraphic.h>
#include <GG/TextControl.h>

#include <boost/lexical_cast.hpp>

namespace {
    const std::map<int, std::string>& StarTypesNames()
    {
        static std::map<int, std::string> star_type_names;
        star_type_names[STAR_BLUE] = "blue";
        star_type_names[STAR_WHITE] = "white";
        star_type_names[STAR_YELLOW] = "yellow";
        star_type_names[STAR_ORANGE] = "orange";
        star_type_names[STAR_RED] = "red";
        star_type_names[STAR_NEUTRON] = "neutron";
        star_type_names[STAR_BLACK] = "black";
        return star_type_names;
    }

}

////////////////////////////////////////////////
// SystemIcon
////////////////////////////////////////////////
SystemIcon::SystemIcon(int id, double zoom) :
    GG::Control(0, 0, 1, 1, GG::CLICKABLE),
    m_system(*GetUniverse().Object<const System>(id)),
    m_static_graphic(0),
    m_default_star_color(GG::CLR_WHITE)
{
    Connect(m_system.StateChangedSignal, &SystemIcon::Refresh, this);

    SetText(m_system.Name());

    //resize to the proper size
    GG::Pt ul(static_cast<int>((m_system.X() - ClientUI::SYSTEM_ICON_SIZE / 2) * zoom),
              static_cast<int>((m_system.Y() - ClientUI::SYSTEM_ICON_SIZE / 2) * zoom));
    SizeMove(ul,
             GG::Pt(static_cast<int>(ul.x + ClientUI::SYSTEM_ICON_SIZE * zoom + 0.5),
                    static_cast<int>(ul.y + ClientUI::SYSTEM_ICON_SIZE * zoom + 0.5)));

    // star graphic
    //boost::shared_ptr<GG::Texture> graphic = GetStarTexture(m_system.Star(), m_system.ID());
    boost::shared_ptr<GG::Texture> graphic = ClientUI::GetNumberedTexture("stars", StarTypesNames(), m_system.Star(), m_system.ID());

    //setup static graphic
    m_static_graphic = new GG::StaticGraphic(0, 0, Width(), Height(), graphic, GG::GR_FITGRAPHIC);
    AdjustBrightness(m_default_star_color, 0.80);
    m_static_graphic->SetColor(m_default_star_color);
    AttachChild(m_static_graphic);
}


SystemIcon::~SystemIcon()
{}

const System& SystemIcon::GetSystem() const
{
    return m_system;
}

const FleetButton* SystemIcon::GetFleetButton(Fleet* fleet) const
{
    std::map<int, FleetButton*>::const_iterator it = m_stationary_fleet_markers.find(*fleet->Owners().begin());
    if (it != m_stationary_fleet_markers.end()) {
        const std::vector<Fleet*>& fleets = it->second->Fleets();
        if (std::find(fleets.begin(), fleets.end(), fleet) != fleets.end())
            return it->second;
    }
    it = m_moving_fleet_markers.find(*fleet->Owners().begin());
    if (it != m_moving_fleet_markers.end()) {
        const std::vector<Fleet*>& fleets = it->second->Fleets();
        if (std::find(fleets.begin(), fleets.end(), fleet) != fleets.end())
            return it->second;
    }
    return 0;
}

void SystemIcon::SizeMove(const GG::Pt& ul, const GG::Pt& lr)
{
    Wnd::SizeMove(ul, lr);
    if (m_static_graphic)
        m_static_graphic->SizeMove(GG::Pt(0, 0), lr - ul);
    PositionSystemName();

    const int BUTTON_SIZE = static_cast<int>(Height() * ClientUI::FLEET_BUTTON_SIZE);
    GG::Pt size = Size();
    int stationary_y = 0;
    for (std::map<int, FleetButton*>::iterator it = m_stationary_fleet_markers.begin(); it != m_stationary_fleet_markers.end(); ++it) {
        it->second->SizeMove(GG::Pt(size.x - BUTTON_SIZE, stationary_y), GG::Pt(size.x, stationary_y + BUTTON_SIZE));
        stationary_y += BUTTON_SIZE;
    }
    int moving_y = size.y - BUTTON_SIZE;
    for (std::map<int, FleetButton*>::iterator it = m_moving_fleet_markers.begin(); it != m_moving_fleet_markers.end(); ++it) {
        it->second->SizeMove(GG::Pt(0, moving_y), GG::Pt(BUTTON_SIZE, moving_y + BUTTON_SIZE));
        moving_y -= BUTTON_SIZE;
    }
}

void SystemIcon::LClick(const GG::Pt& pt, Uint32 keys)
{
    if (!Disabled())
        LeftClickedSignal(m_system.ID());
}

void SystemIcon::RClick(const GG::Pt& pt, Uint32 keys)
{
    if (!Disabled())
        RightClickedSignal(m_system.ID());
}

void SystemIcon::LDoubleClick(const GG::Pt& pt, Uint32 keys)
{
    if (!Disabled())
        LeftDoubleClickedSignal(m_system.ID());
}

void SystemIcon::MouseEnter(const GG::Pt& pt, Uint32 keys)
{
    m_static_graphic->SetColor(GG::CLR_WHITE);
    MouseEnteringSignal(m_system.ID());
}

void SystemIcon::MouseLeave()
{
    m_static_graphic->SetColor(m_default_star_color);
    MouseLeavingSignal(m_system.ID());
}

void SystemIcon::Refresh()
{
    SetText(m_system.Name());

    // set up the name text controls
    for (unsigned int i = 0; i < m_name.size(); ++i) {
        DeleteChild(m_name[i]);
    }
    m_name.clear();

    const std::set<int>& owners = m_system.Owners();
    if (owners.size() < 2) {
        GG::Clr text_color = ClientUI::TEXT_COLOR;
        if (!owners.empty()) {
            text_color = Empires().Lookup(*owners.begin())->Color();
        }
        m_name.push_back(new GG::TextControl(0, 0, m_system.Name(), GG::GUI::GetGUI()->GetFont(ClientUI::FONT, ClientUI::PTS), text_color));
        AttachChild(m_name[0]);
    } else {
        boost::shared_ptr<GG::Font> font = GG::GUI::GetGUI()->GetFont(ClientUI::FONT, ClientUI::PTS);
        Uint32 format = 0;
        std::vector<GG::Font::LineData> lines;
        GG::Pt extent = font->DetermineLines(m_system.Name(), format, 1000, lines);
        unsigned int first_char_pos = 0;
        unsigned int last_char_pos = 0;
        int pixels_per_owner = extent.x / owners.size() + 1; // the +1 is to make sure there is not a stray character left off the end
        int owner_idx = 1;
        for (std::set<int>::const_iterator it = owners.begin(); it != owners.end(); ++it, ++owner_idx) {
            while (last_char_pos < m_system.Name().size() && lines[0].char_data[last_char_pos].extent < (owner_idx * pixels_per_owner)) {
                ++last_char_pos;
            }
            m_name.push_back(new GG::TextControl(0, 0, m_system.Name().substr(first_char_pos, last_char_pos - first_char_pos), 
                                                 GG::GUI::GetGUI()->GetFont(ClientUI::FONT, ClientUI::PTS),
                                                 Empires().Lookup(*it)->Color()));
            AttachChild(m_name.back());
            first_char_pos = last_char_pos;
        }
    }
    PositionSystemName();

    std::vector<const Fleet*> fleets = m_system.FindObjects<Fleet>();
    for (unsigned int i = 0; i < fleets.size(); ++i)
        Connect(fleets[i]->StateChangedSignal, &SystemIcon::CreateFleetButtons, this);
    Connect(m_system.FleetAddedSignal, &SystemIcon::FleetCreatedOrDestroyed, this);
    Connect(m_system.FleetRemovedSignal, &SystemIcon::FleetCreatedOrDestroyed, this);

    CreateFleetButtons();
}

void SystemIcon::ClickFleetButton(Fleet* fleet)
{
    for (std::map<int, FleetButton*>::iterator it = m_stationary_fleet_markers.begin(); it != m_stationary_fleet_markers.end(); ++it) {
        if (std::find(it->second->Fleets().begin(), it->second->Fleets().end(), fleet) != it->second->Fleets().end()) {
            it->second->SelectFleet(fleet);
            it->second->LClick(GG::Pt(), 0);
            return;
        }
    }
    for (std::map<int, FleetButton*>::iterator it = m_moving_fleet_markers.begin(); it != m_moving_fleet_markers.end(); ++it) {
        if (std::find(it->second->Fleets().begin(), it->second->Fleets().end(), fleet) != it->second->Fleets().end()) {
            it->second->SelectFleet(fleet);
            it->second->LClick(GG::Pt(), 0);
            return;
        }
    }
}

void SystemIcon::ShowName()
{
    for (unsigned int i = 0; i < m_name.size(); ++i) {
        m_name[i]->Show();
    }
}

void SystemIcon::HideName()
{
    for (unsigned int i = 0; i < m_name.size(); ++i) {
        m_name[i]->Hide();
    }
}

void SystemIcon::CreateFleetButtons()
{
    // clear out old fleet buttons
    for (std::map<int, FleetButton*>::iterator it = m_stationary_fleet_markers.begin(); it != m_stationary_fleet_markers.end(); ++it)
        DeleteChild(it->second);
    for (std::map<int, FleetButton*>::iterator it = m_moving_fleet_markers.begin(); it != m_moving_fleet_markers.end(); ++it)
        DeleteChild(it->second);
    m_stationary_fleet_markers.clear();
    m_moving_fleet_markers.clear();

    const int BUTTON_SIZE = static_cast<int>(Height() * ClientUI::FLEET_BUTTON_SIZE);
    GG::Pt size = Size();
    MapWnd* map_wnd = ClientUI::GetClientUI()->GetMapWnd();
    int stationary_y = 0;
    int moving_y = size.y - BUTTON_SIZE;
    for (EmpireManager::const_iterator it = Empires().begin(); it != Empires().end(); ++it) {
        std::vector<int> fleet_IDs = m_system.FindObjectIDs(StationaryFleetVisitor(it->first));
        FleetButton* stationary_fb = 0;
        if (!fleet_IDs.empty()) {
            stationary_fb = new FleetButton(size.x - BUTTON_SIZE, stationary_y, BUTTON_SIZE, BUTTON_SIZE, it->second->Color(), fleet_IDs, SHAPE_LEFT);
            m_stationary_fleet_markers[it->first] = stationary_fb;
            AttachChild(m_stationary_fleet_markers[it->first]);
            map_wnd->SetFleetMovement(stationary_fb);
            stationary_y += BUTTON_SIZE;
        }
        fleet_IDs = m_system.FindObjectIDs(OrderedMovingFleetVisitor(it->first));
        FleetButton* moving_fb = 0;
        if (!fleet_IDs.empty()) {
            moving_fb = new FleetButton(0, moving_y, BUTTON_SIZE, BUTTON_SIZE, it->second->Color(), fleet_IDs, SHAPE_RIGHT);
            m_moving_fleet_markers[it->first] = moving_fb;
            AttachChild(m_moving_fleet_markers[it->first]);
            map_wnd->SetFleetMovement(moving_fb);
            moving_y -= BUTTON_SIZE;
        }
        if (stationary_fb && moving_fb) {
            moving_fb->SetCompliment(stationary_fb);
            stationary_fb->SetCompliment(moving_fb);
        }
    }
}

void SystemIcon::PositionSystemName()
{
    if (!m_name.empty()) {
        int total_width = 0;
        std::vector<int> extents;
        for (unsigned int i = 0; i < m_name.size(); ++i) {
            extents.push_back(total_width);
            total_width += m_name[i]->Width();
        }
        for (unsigned int i = 0; i < m_name.size(); ++i) {
            m_name[i]->MoveTo(GG::Pt((Width() - total_width) / 2 + extents[i], Height()));
        }
    }
}

void SystemIcon::FleetCreatedOrDestroyed(const Fleet&)
{
    CreateFleetButtons();
}
