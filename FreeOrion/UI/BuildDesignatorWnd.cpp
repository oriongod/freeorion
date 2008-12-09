#include "BuildDesignatorWnd.h"

#include "CUIControls.h"
#include "ClientUI.h"
#include "CUISpin.h"
#include "CUIWnd.h"
#include "SidePanel.h"
#include "TechTreeWnd.h"
#include "MapWnd.h"
#include "../util/MultiplayerCommon.h"
#include "../universe/UniverseObject.h"
#include "../Empire/Empire.h"
#include "../universe/Building.h"
#include "../universe/ShipDesign.h"
#include "../universe/Effect.h"

#include <GG/DrawUtil.h>
#include <GG/Layout.h>
#include <GG/StaticGraphic.h>

#include <boost/format.hpp>

namespace {
    class BuildableItemsListBox : public CUIListBox
    {
    public:
        BuildableItemsListBox(GG::X x, GG::Y y, GG::X w, GG::Y h) :
            CUIListBox(x, y, w, h)
        {}
        virtual void GainingFocus() {DeselectAll();}
    };

    struct ToggleBuildTypeFunctor
    {
        ToggleBuildTypeFunctor(BuildDesignatorWnd* designator_wnd, BuildType type) : m_designator_wnd(designator_wnd), m_build_type(type) {}
        void operator()() {m_designator_wnd->ToggleType(m_build_type);}
        BuildDesignatorWnd* const m_designator_wnd;
        const BuildType m_build_type;
    };

    struct ToggleAllBuildTypesFunctor
    {
        ToggleAllBuildTypesFunctor(BuildDesignatorWnd* designator_wnd) : m_designator_wnd(designator_wnd) {}
        void operator()() {m_designator_wnd->ToggleAllTypes();}
        BuildDesignatorWnd* const m_designator_wnd;
    };

    struct ToggleAvailabilityFunctor
    {
        ToggleAvailabilityFunctor(BuildDesignatorWnd* designator_wnd, bool available) : m_designator_wnd(designator_wnd), m_available(available) {}
        void operator()() {m_designator_wnd->ToggleAvailabilitly(m_available);}
        BuildDesignatorWnd* const m_designator_wnd;
        const bool m_available; // true: toggle whether to show available techs; false: toggle whether to show unavailable techs
    };
}

//////////////////////////////////////////////////
// BuildDesignatorWnd::BuildDetailPanel
//////////////////////////////////////////////////
class BuildDesignatorWnd::BuildDetailPanel : public CUIWnd
{
public:
    BuildDetailPanel(GG::X w, GG::Y h);

    void SizeMove(const GG::Pt& ul, const GG::Pt& lr);
    void Render();
    void LDrag(const GG::Pt& pt, const GG::Pt& move, GG::Flags<GG::ModKey> mod_keys);

    /* need to redefine this so that icons and name can be put at the top of the Wnd, rather
       than being restricted to the client area of a CUIWnd */
    GG::Pt ClientUpperLeft() const;

    void SetBuildItem(BuildType build_type, const std::string& item = "");
    void SetBuildItem(BuildType build_type, int design_id);
    void SetBuild(int queue_idx);

private:
    static const GG::X TEXT_MARGIN_X;
    static const GG::Y TEXT_MARGIN_Y;

    void Reset();
    void DoLayout();

    bool DisplayingQueueItem() const;

    BuildType           m_build_type;
    std::string         m_item_name;
    int                 m_item_design_id;
    GG::TextControl*    m_item_name_text;
    GG::TextControl*    m_cost_text;
    GG::TextControl*    m_summary_text;
    CUIMultiEdit*       m_description_box;
    GG::StaticGraphic*  m_item_graphic;
};
const GG::X BuildDesignatorWnd::BuildDetailPanel::TEXT_MARGIN_X(3);
const GG::Y BuildDesignatorWnd::BuildDetailPanel::TEXT_MARGIN_Y(3);

BuildDesignatorWnd::BuildDetailPanel::BuildDetailPanel(GG::X w, GG::Y h) :
    CUIWnd("", GG::X1, GG::Y1, w - 1, h - 1, GG::CLICKABLE | GG::DRAGABLE | GG::RESIZABLE | GG::ONTOP),
    m_build_type(INVALID_BUILD_TYPE),
    m_item_name("")
{
    const int PTS = ClientUI::Pts();
    const int NAME_PTS = PTS*3/2;
    const int COST_PTS = PTS;
    const int SUMMARY_PTS = PTS*4/3;

    m_item_name_text = new GG::TextControl(GG::X0, GG::Y0, GG::X(10), GG::Y(10), "", ClientUI::GetBoldFont(NAME_PTS), ClientUI::TextColor());
    m_cost_text =      new GG::TextControl(GG::X0, GG::Y0, GG::X(10), GG::Y(10), "", ClientUI::GetFont(COST_PTS),     ClientUI::TextColor());
    m_summary_text =   new GG::TextControl(GG::X0, GG::Y0, GG::X(10), GG::Y(10), "", ClientUI::GetFont(SUMMARY_PTS),  ClientUI::TextColor());
    m_description_box = new CUILinkTextMultiEdit(GG::X0, GG::Y0, GG::X(10), GG::Y(10), "", GG::MULTI_WORDBREAK | GG::MULTI_READ_ONLY);
    m_description_box->SetColor(GG::CLR_ZERO);
    m_description_box->SetInteriorColor(GG::CLR_ZERO);

    m_item_graphic = 0;

    AttachChild(m_item_name_text);
    AttachChild(m_cost_text);
    AttachChild(m_summary_text);
    AttachChild(m_description_box);

    DoLayout();
}

void BuildDesignatorWnd::BuildDetailPanel::DoLayout()
{
    const int PTS = ClientUI::Pts();
    const int NAME_PTS = PTS*3/2;
    const int COST_PTS = PTS;
    const int SUMMARY_PTS = PTS*4/3;

    const int ICON_SIZE = 12 + NAME_PTS + COST_PTS + SUMMARY_PTS;

    // name
    GG::Pt ul = GG::Pt();
    GG::Pt lr = ul + GG::Pt(Width(), GG::Y(NAME_PTS + 4));
    m_item_name_text->SizeMove(ul, lr);

    // cost / turns
    ul += GG::Pt(GG::X0, m_item_name_text->Height());
    lr = ul + GG::Pt(Width(), GG::Y(COST_PTS + 4));
    m_cost_text->SizeMove(ul, lr);

    // one line summary
    ul += GG::Pt(GG::X0, m_cost_text->Height());
    lr = ul + GG::Pt(Width(), GG::Y(SUMMARY_PTS + 4));
    m_summary_text->SizeMove(ul, lr);

    // main verbose description (fluff, effects, unlocks, ...)
    ul = GG::Pt(GG::X1, ICON_SIZE + TEXT_MARGIN_Y + 1);
    lr = ul + GG::Pt(Width() - TEXT_MARGIN_X - BORDER_RIGHT, Height() - BORDER_BOTTOM - ul.y - TEXT_MARGIN_Y);
    m_description_box->SizeMove(ul, lr);

    // icon
    if (m_item_graphic) {
        ul = GG::Pt(GG::X1, GG::Y1);
        lr = ul + GG::Pt(GG::X(ICON_SIZE), GG::Y(ICON_SIZE));
        m_item_graphic->SizeMove(ul, lr);
    }
}

void BuildDesignatorWnd::BuildDetailPanel::SizeMove(const GG::Pt& ul, const GG::Pt& lr)
{
    GG::Pt old_size = GG::Wnd::LowerRight() - GG::Wnd::UpperLeft();

    // maybe later do something interesting with docking
    GG::Wnd::SizeMove(ul, lr);

    if (Visible() && old_size != GG::Wnd::Size())
        DoLayout();
}

GG::Pt BuildDesignatorWnd::BuildDetailPanel::ClientUpperLeft() const
{
    return GG::Wnd::UpperLeft();
}

void BuildDesignatorWnd::BuildDetailPanel::Render()
{
    GG::Pt ul = UpperLeft();
    GG::Pt lr = LowerRight();
    const GG::Y ICON_HEIGHT = m_summary_text->LowerRight().y - m_item_name_text->UpperLeft().y;
    GG::Pt cl_ul = ul + GG::Pt(BORDER_LEFT, ICON_HEIGHT + BORDER_BOTTOM);
    GG::Pt cl_lr = lr - GG::Pt(BORDER_RIGHT, BORDER_BOTTOM);

   // use GL to draw the lines
    glDisable(GL_TEXTURE_2D);
    GLint initial_modes[2];
    glGetIntegerv(GL_POLYGON_MODE, initial_modes);

    // draw background
    glPolygonMode(GL_BACK, GL_FILL);
    glBegin(GL_POLYGON);
        glColor(ClientUI::WndColor());
        glVertex(ul.x, ul.y);
        glVertex(lr.x, ul.y);
        glVertex(lr.x, lr.y - OUTER_EDGE_ANGLE_OFFSET);
        glVertex(lr.x - OUTER_EDGE_ANGLE_OFFSET, lr.y);
        glVertex(ul.x, lr.y);
        glVertex(ul.x, ul.y);
    glEnd();

    // draw outer border on pixel inside of the outer edge of the window
    glPolygonMode(GL_BACK, GL_LINE);
    glBegin(GL_POLYGON);
        glColor(ClientUI::WndOuterBorderColor());
        glVertex(ul.x, ul.y);
        glVertex(lr.x, ul.y);
        glVertex(lr.x, lr.y - OUTER_EDGE_ANGLE_OFFSET);
        glVertex(lr.x - OUTER_EDGE_ANGLE_OFFSET, lr.y);
        glVertex(ul.x, lr.y);
        glVertex(ul.x, ul.y);
    glEnd();

    // reset this to whatever it was initially
    glPolygonMode(GL_BACK, initial_modes[1]);

    // draw inner border, including extra resize-tab lines
    glBegin(GL_LINE_STRIP);
        glColor(ClientUI::WndInnerBorderColor());
        glVertex(cl_ul.x, cl_ul.y);
        glVertex(cl_lr.x, cl_ul.y);
        glVertex(cl_lr.x, cl_lr.y - INNER_BORDER_ANGLE_OFFSET);
        glVertex(cl_lr.x - INNER_BORDER_ANGLE_OFFSET, cl_lr.y);
        glVertex(cl_ul.x, cl_lr.y);
        glVertex(cl_ul.x, cl_ul.y);
    glEnd();
    glBegin(GL_LINES);
        // draw the extra lines of the resize tab
        glColor(ClientUI::WndInnerBorderColor());
        glVertex(cl_lr.x, cl_lr.y - RESIZE_HASHMARK1_OFFSET);
        glVertex(cl_lr.x - RESIZE_HASHMARK1_OFFSET, cl_lr.y);
        
        glVertex(cl_lr.x, cl_lr.y - RESIZE_HASHMARK2_OFFSET);
        glVertex(cl_lr.x - RESIZE_HASHMARK2_OFFSET, cl_lr.y);
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

void BuildDesignatorWnd::BuildDetailPanel::LDrag(const GG::Pt& pt, const GG::Pt& move, GG::Flags<GG::ModKey> mod_keys)
{
    if (m_drag_offset != GG::Pt(-GG::X1, -GG::Y1)) {  // resize-dragging
        GG::Pt new_lr = pt - m_drag_offset;

        // constrain to within parent
        if (GG::Wnd* parent = Parent()) {
            GG::Pt max_lr = parent->ClientLowerRight();
            new_lr.x = std::min(new_lr.x, max_lr.x);
            new_lr.y = std::min(new_lr.y, max_lr.y);
        }        

        Resize(new_lr - UpperLeft());
    } else {    // normal-dragging
        GG::Pt final_move = move;

        if (GG::Wnd* parent = Parent()) {
            GG::Pt ul = UpperLeft(), lr = LowerRight();
            GG::Pt new_ul = ul + move, new_lr = lr + move;

            GG::Pt min_ul = parent->ClientUpperLeft() + GG::Pt(GG::X1, GG::Y1);
            GG::Pt max_lr = parent->ClientLowerRight();
            GG::Pt max_ul = max_lr - this->Size();

            new_ul.x = std::max(min_ul.x, std::min(max_ul.x, new_ul.x));
            new_ul.y = std::max(min_ul.y, std::min(max_ul.y, new_ul.y));

            final_move = new_ul - ul;
        }

        GG::Wnd::LDrag(pt, final_move, mod_keys);
    }
}

void BuildDesignatorWnd::BuildDetailPanel::SetBuildItem(BuildType build_type, const std::string& item)
{
    if (build_type != BT_BUILDING && build_type != INVALID_BUILD_TYPE)
        throw std::invalid_argument("Attempted to SetBuildItem with a name and BuildType that wasn't BT_BUILDING");
    m_build_type = build_type;
    m_item_name = item;
    m_item_design_id = UniverseObject::INVALID_OBJECT_ID;
    Reset();
}

void BuildDesignatorWnd::BuildDetailPanel::SetBuildItem(BuildType build_type, int design_id)
{
    if (build_type != BT_SHIP)
        throw std::invalid_argument("Attempted to SetBuildItem with a design id and BuildType that wasn't BT_SHIP");
    m_build_type = build_type;
    m_item_name = "";
    m_item_design_id = design_id;
    Reset();
}

void BuildDesignatorWnd::BuildDetailPanel::SetBuild(int queue_idx)
{
    Empire* empire = Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
    const ProductionQueue& queue = empire->GetProductionQueue();
    if (0 <= queue_idx && queue_idx < static_cast<int>(queue.size())) {
        m_build_type = queue[queue_idx].item.build_type;
        m_item_name = queue[queue_idx].item.name;
        m_item_design_id = queue[queue_idx].item.design_id;
    } else {
        m_build_type = INVALID_BUILD_TYPE;
        m_item_name = "";
        m_item_design_id = UniverseObject::INVALID_OBJECT_ID;
    }
    Reset();
}

void BuildDesignatorWnd::BuildDetailPanel::Reset()
{
    m_item_name_text->SetText("");
    m_cost_text->SetText("");
    m_description_box->SetText("");
    if (m_item_graphic) {
        DeleteChild(m_item_graphic);
        m_item_graphic = 0;
    }

    if (m_build_type == INVALID_BUILD_TYPE) return;

    Empire* empire = Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
    if (!empire) return;

    using boost::io::str;
    using boost::format;
    double cost_per_turn = 0;
    int turns = 0;
    std::string item_name_str = UserString(m_item_name);
    std::string description_str;
    boost::shared_ptr<GG::Texture> graphic;
    if (m_build_type == BT_BUILDING) {
        assert(empire);
        const BuildingType* building_type = GetBuildingType(m_item_name);
        assert(building_type);
        turns = building_type->BuildTime();
        boost::tie(cost_per_turn, turns) = empire->ProductionCostAndTime(BT_BUILDING, m_item_name);
        if (building_type->Effects().empty()) {
            description_str = str(format(UserString("PRODUCTION_DETAIL_BUILDING_DESCRIPTION_STR"))
                                  % UserString(building_type->Description()));
        } else {
            description_str = str(format(UserString("PRODUCTION_DETAIL_BUILDING_DESCRIPTION_STR_WITH_EFFECTS"))
                                  % UserString(building_type->Description())
                                  % EffectsDescription(building_type->Effects()));
        }
        graphic = ClientUI::BuildingTexture(building_type->Name());
    } else if (m_build_type == BT_SHIP) {
        assert(empire);
        const ShipDesign* design = GetShipDesign(m_item_design_id);
        assert(design);
        turns = 5; // this is a kludge for v0.3 only
        boost::tie(cost_per_turn, turns) = empire->ProductionCostAndTime(BT_SHIP, m_item_design_id);
        item_name_str = design->Name();
        description_str = str(format(UserString("PRODUCTION_DETAIL_SHIP_DESCRIPTION_STR"))
                              % design->Description()
                              % design->Attack()
                              % design->Defense()
                              % design->Speed());
        graphic = ClientUI::ShipIcon(design->ID());
    }

    if (graphic) {
        GG::Pt ul = ClientUpperLeft();
        m_item_graphic = new GG::StaticGraphic(GG::X0, GG::Y0, GG::X(10), GG::Y(10),
                                               graphic, GG::GRAPHIC_FITGRAPHIC | GG::GRAPHIC_PROPSCALE);
        m_item_graphic->Show();
        AttachChild(m_item_graphic);
    }

    DoLayout();

    m_item_name_text->SetText(item_name_str);
    m_cost_text->SetText(str(format(UserString("PRODUCTION_TOTAL_COST_STR"))
                             % static_cast<int>(cost_per_turn + 0.5)
                             % turns));
    m_description_box->SetText(description_str);
}


//////////////////////////////////////////////////
// BuildDesignatorWnd::BuildSelector
//////////////////////////////////////////////////
class BuildDesignatorWnd::BuildSelector : public CUIWnd
{
public:
    BuildSelector(GG::X w, GG::Y h);

    void SizeMove(const GG::Pt& ul, const GG::Pt& lr);
    void LDrag(const GG::Pt& pt, const GG::Pt& move, GG::Flags<GG::ModKey> mod_keys);

    const std::set<BuildType>&      GetBuildTypesShown() const;
    const std::pair<bool, bool>&    GetAvailabilitiesShown() const; // .first -> available items; .second -> unavailable items

    void MinimizeClicked();

    void SetBuildLocation(int location_id);

    void Reset();

    void ShowType(BuildType type, bool refresh_list = true);
    void ShowAllTypes(bool refresh_list = true);
    void HideType(BuildType type, bool refresh_list = true);
    void HideAllTypes(bool refresh_list = true);

    void ShowAvailability(bool available, bool refresh_list = true);
    void HideAvailability(bool available, bool refresh_list = true);

    mutable boost::signal<void (BuildType, const std::string&)>         DisplayNamedBuildItemSignal;
    mutable boost::signal<void (BuildType, const std::string&, int)>    RequestNamedBuildItemSignal;
    mutable boost::signal<void (BuildType, int)>                        DisplayIDedBuildItemSignal;
    mutable boost::signal<void (BuildType, int, int)>                   RequestIDedBuildItemSignal;

private:
    typedef std::map<
        GG::ListBox::iterator,
        BuildType,
        GG::ListBox::RowPtrIteratorLess<GG::ListBox>
     > RowToBuildTypeMap;

    static const GG::X TEXT_MARGIN_X;
    static const GG::Y TEXT_MARGIN_Y;

    void DoLayout();

    bool BuildableItemVisible(BuildType build_type, const std::string& name);
    bool BuildableItemVisible(BuildType build_type, int design_id);

    void PopulateList();
    std::vector<GG::X> ColWidths();

    void BuildItemSelected(const GG::ListBox::SelectionSet& selections);
    void BuildItemDoubleClicked(GG::ListBox::iterator it);

    std::map<BuildType, CUIButton*>         m_build_type_buttons;
    std::vector<CUIButton*>                 m_availability_buttons;

    std::set<BuildType>                     m_build_types_shown;
    std::pair<bool, bool>                   m_availabilities_shown; // .first -> available items; .second -> unavailable items

    BuildableItemsListBox*                  m_buildable_items;
    RowToBuildTypeMap                       m_build_types;
    GG::Pt                                  m_original_ul;

    int                                     m_build_location;

    GG::Y                              m_row_height;

    friend class BuildDesignatorWnd;        // so BuildDesignatorWnd can access buttons
};
const GG::X BuildDesignatorWnd::BuildSelector::TEXT_MARGIN_X(3);
const GG::Y BuildDesignatorWnd::BuildSelector::TEXT_MARGIN_Y(3);

BuildDesignatorWnd::BuildSelector::BuildSelector(GG::X w, GG::Y h) :
    CUIWnd(UserString("PRODUCTION_WND_BUILD_ITEMS_TITLE"), GG::X1, GG::Y1, w - 1, h - 1,
           GG::CLICKABLE | GG::DRAGABLE | GG::RESIZABLE | GG::ONTOP),
    m_buildable_items(new BuildableItemsListBox(GG::X0, GG::Y0, GG::X1, GG::Y1)),
    m_build_types(GG::ListBox::RowPtrIteratorLess<GG::ListBox>(m_buildable_items)),
    m_build_location(UniverseObject::INVALID_OBJECT_ID)
{
    // create build type toggle buttons (ship, building, orbital, all)
    m_build_type_buttons[BT_BUILDING] = new CUIButton(GG::X0, GG::Y0, GG::X1, UserString("PRODUCTION_WND_CATEGORY_BT_BUILDING"));
    AttachChild(m_build_type_buttons[BT_BUILDING]);
    m_build_type_buttons[BT_SHIP] = new CUIButton(GG::X0, GG::Y0, GG::X1, UserString("PRODUCTION_WND_CATEGORY_BT_SHIP"));
    AttachChild(m_build_type_buttons[BT_SHIP]);
    m_build_type_buttons[NUM_BUILD_TYPES] = new CUIButton(GG::X0, GG::Y0, GG::X1, UserString("ALL"));
    AttachChild(m_build_type_buttons[NUM_BUILD_TYPES]);

    // create availability toggle buttons (available, not available)
    m_availability_buttons.push_back(new CUIButton(GG::X0, GG::Y0, GG::X1, UserString("PRODUCTION_WND_AVAILABILITY_AVAILABLE")));
    AttachChild(m_availability_buttons.back());
    m_availability_buttons.push_back(new CUIButton(GG::X0, GG::Y0, GG::X1, UserString("PRODUCTION_WND_AVAILABILITY_UNAVAILABLE")));
    AttachChild(m_availability_buttons.back());

    // selectable list of buildable items
    AttachChild(m_buildable_items);
    GG::Connect(m_buildable_items->SelChangedSignal,
                &BuildDesignatorWnd::BuildSelector::BuildItemSelected, this);
    GG::Connect(m_buildable_items->DoubleClickedSignal,
                &BuildDesignatorWnd::BuildSelector::BuildItemDoubleClicked, this);
    m_buildable_items->SetStyle(GG::LIST_NOSORT | GG::LIST_SINGLESEL);

    m_row_height = GG::Y(ClientUI::Pts()*3/2);
    std::vector<GG::X> col_widths = ColWidths();


    //GG::ListBox::Row* header = new GG::ListBox::Row();
    //boost::shared_ptr<GG::Font> font = ClientUI::GetFont();
    //GG::Clr clr = ClientUI::TextColor();
    //header->push_back("item", font, clr);
    //header->push_back("PP/turn", font, clr);
    //header->push_back("turns", font, clr);
    //header->push_back("description", font, clr);
    //header->SetColWidths(col_widths);
    //m_buildable_items->SetColHeaders(header);


    m_buildable_items->SetNumCols(static_cast<int>(col_widths.size()));
    m_buildable_items->LockColWidths();

    for (unsigned int i = 0; i < col_widths.size(); ++i) {
        m_buildable_items->SetColWidth(i, col_widths[i]);
        m_buildable_items->SetColAlignment(i, GG::ALIGN_LEFT);
    }

    DoLayout();
}

const std::set<BuildType>& BuildDesignatorWnd::BuildSelector::GetBuildTypesShown() const
{
   return m_build_types_shown;
}

const std::pair<bool, bool>& BuildDesignatorWnd::BuildSelector::GetAvailabilitiesShown() const
{
    return m_availabilities_shown;
}

void BuildDesignatorWnd::BuildSelector::DoLayout()
{
    int num_buttons = std::max(1, static_cast<int>(m_build_type_buttons.size() + m_availability_buttons.size()));
    GG::X x(0);
    GG::X button_width = ClientWidth() / num_buttons;
    GG::Y button_height(20);
    for (unsigned int i = 0; i < m_build_type_buttons.size() - 1; ++i) {
        m_build_type_buttons[BuildType(BT_BUILDING + i)]->SizeMove(GG::Pt(x, GG::Y0), GG::Pt(x + button_width, button_height));
        x += button_width;
    }
    m_build_type_buttons[NUM_BUILD_TYPES]->SizeMove(GG::Pt(x, GG::Y0), GG::Pt(x + button_width, button_height)); x += button_width;

    m_availability_buttons[0]->SizeMove(GG::Pt(x, GG::Y0), GG::Pt(x + button_width, button_height)); x += button_width;
    m_availability_buttons[1]->SizeMove(GG::Pt(x, GG::Y0), GG::Pt(x + button_width, button_height));

    m_buildable_items->SizeMove(GG::Pt(GG::X0, button_height), ClientSize() - GG::Pt(TEXT_MARGIN_X, TEXT_MARGIN_Y));
}

void BuildDesignatorWnd::BuildSelector::SizeMove(const GG::Pt& ul, const GG::Pt& lr)
{
    GG::Pt old_size = GG::Wnd::LowerRight() - GG::Wnd::UpperLeft();

    // maybe later do something interesting with docking
    GG::Wnd::SizeMove(ul, lr);

    if (Visible() && old_size != GG::Wnd::Size())
        DoLayout();
}

void BuildDesignatorWnd::BuildSelector::LDrag(const GG::Pt& pt, const GG::Pt& move, GG::Flags<GG::ModKey> mod_keys)
{
    if (m_drag_offset != GG::Pt(-GG::X1, -GG::Y1)) {  // resize-dragging
        GG::Pt new_lr = pt - m_drag_offset;

        // constrain to within parent
        if (GG::Wnd* parent = Parent()) {
            GG::Pt max_lr = parent->ClientLowerRight();
            new_lr.x = std::min(new_lr.x, max_lr.x);
            new_lr.y = std::min(new_lr.y, max_lr.y);
        }        

        Resize(new_lr - UpperLeft());
    } else {    // normal-dragging
        GG::Pt final_move = move;

        if (GG::Wnd* parent = Parent()) {
            GG::Pt ul = UpperLeft(), lr = LowerRight();
            GG::Pt new_ul = ul + move, new_lr = lr + move;

            GG::Pt min_ul = parent->ClientUpperLeft() + GG::Pt(GG::X1, GG::Y1);
            GG::Pt max_lr = parent->ClientLowerRight();
            GG::Pt max_ul = max_lr - this->Size();

            new_ul.x = std::max(min_ul.x, std::min(max_ul.x, new_ul.x));
            new_ul.y = std::max(min_ul.y, std::min(max_ul.y, new_ul.y));

            final_move = new_ul - ul;
        }

        GG::Wnd::LDrag(pt, final_move, mod_keys);
    }
}

void BuildDesignatorWnd::BuildSelector::MinimizeClicked()
{
    if (!m_minimized) {
        m_minimized = true;
        m_original_size = Size();
        m_original_ul = RelativeUpperLeft();
        GG::Pt original_lr = m_original_ul + m_original_size;
        GG::Pt new_size(Width(), BORDER_TOP);
        SetMinSize(GG::Pt(new_size.x, new_size.y));
        SizeMove(original_lr - new_size, original_lr);
        GG::Pt button_ul = GG::Pt(Width() - BUTTON_RIGHT_OFFSET, BUTTON_TOP_OFFSET);
        if (m_close_button)
            m_close_button->MoveTo(GG::Pt(button_ul.x, button_ul.y));
        if (m_minimize_button)
            m_minimize_button->MoveTo(GG::Pt(button_ul.x - (m_close_button ? BUTTON_RIGHT_OFFSET : GG::X0), button_ul.y));
        Hide();
        Show(false);
        if (m_close_button)
            m_close_button->Show();
        if (m_minimize_button)
            m_minimize_button->Show();
    } else {
        m_minimized = false;
        SetMinSize(GG::Pt(Width(), BORDER_TOP + INNER_BORDER_ANGLE_OFFSET + BORDER_BOTTOM));
        SizeMove(m_original_ul, m_original_ul + m_original_size);
        GG::Pt button_ul = GG::Pt(Width() - BUTTON_RIGHT_OFFSET, BUTTON_TOP_OFFSET) + UpperLeft() - ClientUpperLeft();
        if (m_close_button)
            m_close_button->MoveTo(GG::Pt(button_ul.x, button_ul.y));
        if (m_minimize_button)
            m_minimize_button->MoveTo(GG::Pt(button_ul.x - (m_close_button ? BUTTON_RIGHT_OFFSET : GG::X0), button_ul.y));
        Show();
    }
}

void BuildDesignatorWnd::BuildSelector::SetBuildLocation(int location_id)
{
    m_build_location = location_id;
    PopulateList();
}

void BuildDesignatorWnd::BuildSelector::Reset()
{
    PopulateList();
}

void BuildDesignatorWnd::BuildSelector::ShowType(BuildType type, bool refresh_list)
{
    if (m_build_types_shown.find(type) == m_build_types_shown.end()) {
        m_build_types_shown.insert(type);
        if (refresh_list) PopulateList();
    }
}

void BuildDesignatorWnd::BuildSelector::HideType(BuildType type, bool refresh_list)
{
    std::set<BuildType>::iterator it = m_build_types_shown.find(type);
    if (it != m_build_types_shown.end()) {
        m_build_types_shown.erase(it);
        if (refresh_list) PopulateList();
    }
}

void BuildDesignatorWnd::BuildSelector::ShowAllTypes(bool refresh_list)
{
    m_build_types_shown.insert(BT_BUILDING);
    m_build_types_shown.insert(BT_SHIP);
    if (refresh_list) PopulateList();
}

void BuildDesignatorWnd::BuildSelector::HideAllTypes(bool refresh_list)
{
    m_build_types_shown.clear();
    if (refresh_list) PopulateList();
}

void BuildDesignatorWnd::BuildSelector::ShowAvailability(bool available, bool refresh_list)
{
    if (available) {
        if (!m_availabilities_shown.first) {
            m_availabilities_shown.first = true;
            if (refresh_list) PopulateList();
        }
    } else {
        if (!m_availabilities_shown.second) {
            m_availabilities_shown.second = true;
            if (refresh_list) PopulateList();
        }
    }
}

void BuildDesignatorWnd::BuildSelector::HideAvailability(bool available, bool refresh_list)
{
    if (available) {
        if (m_availabilities_shown.first) {
            m_availabilities_shown.first = false;
            if (refresh_list) PopulateList();
        }
    } else {
        if (m_availabilities_shown.second) {
            m_availabilities_shown.second = false;
            if (refresh_list) PopulateList();
        }
    }
}

bool BuildDesignatorWnd::BuildSelector::BuildableItemVisible(BuildType build_type, const std::string& name)
{
    if (build_type != BT_BUILDING)
        throw std::invalid_argument("BuildableItemVisible was passed an invalid build type with a name");

    if (m_build_types_shown.find(build_type) == m_build_types_shown.end())
        return false;

    bool available = false;

    const Empire* empire = Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
    if (build_type == BT_BUILDING)
        available = empire->BuildingTypeAvailable(name);

    if (available)
        return m_availabilities_shown.first;
    else
        return m_availabilities_shown.second;
}

bool BuildDesignatorWnd::BuildSelector::BuildableItemVisible(BuildType build_type, int design_id)
{
    if (build_type != BT_SHIP)
        throw std::invalid_argument("BuildableItemVisible was passed an invalid build type with an id");

    if (m_build_types_shown.find(build_type) == m_build_types_shown.end())
        return false;

    bool available = false;

    const Empire* empire = Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
    if (build_type == BT_SHIP)
        available = empire->ShipDesignAvailable(design_id);

    if (available)
        return m_availabilities_shown.first;
    else
        return m_availabilities_shown.second;
}

void BuildDesignatorWnd::BuildSelector::PopulateList()
{
    if (!Visible()) return;

    Logger().debugStream() << "PopulateList start";
    Empire* empire = Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
    if (!empire) return;

    // keep track of initially selected row, so that new rows added may be compared to it to see if they should be selected after repopulating
    std::string selected_row;
    if (m_buildable_items->Selections().size() == 1) {
        selected_row = (**m_buildable_items->Selections().begin())->DragDropDataType();
    }


    m_buildable_items->Clear(); // the list of items to be populated
    m_build_types.clear();      // map from row to BuildType


    boost::shared_ptr<GG::Font> default_font = ClientUI::GetFont();

    std::vector<GG::X> col_widths = ColWidths();
    GG::X icon_col_width = col_widths[0];
    GG::X desc_col_width = col_widths[4];

    GG::ListBox::iterator row_to_select = m_buildable_items->end(); // may be set while populating - used to reselect previously selected row after populating
    int i = 0;              // counter that keeps track of how many rows have been added so far

    // populate list with building types
    Logger().debugStream() << "Adding Buildings";
    if (m_build_types_shown.find(BT_BUILDING) != m_build_types_shown.end()) {
        BuildingTypeManager& manager = GetBuildingTypeManager();

        for (BuildingTypeManager::iterator it = manager.begin(); it != manager.end(); ++it, ++i) {
            const BuildingType* type = it->second;
            const std::string name = it->first;

            if (!BuildableItemVisible(BT_BUILDING, name)) continue;

            GG::ListBox::Row* row = new GG::ListBox::Row();
            row->SetDragDropDataType(name);

            // icon
            GG::Control* icon = new GG::StaticGraphic(GG::X0, GG::Y0, icon_col_width, m_row_height, 
                ClientUI::BuildingTexture(type->Name()),
                GG::GRAPHIC_FITGRAPHIC | GG::GRAPHIC_PROPSCALE);
            row->push_back(icon);

            // building name
            row->push_back(UserString(name), default_font, ClientUI::TextColor());

            // cost / turn, and minimum production turns
            const std::pair<double, int> cost_time = empire->ProductionCostAndTime(BT_BUILDING, name);
            std::string cost_text = boost::lexical_cast<std::string>(cost_time.first);
            row->push_back(cost_text, default_font, ClientUI::TextColor());
            std::string time_text = boost::lexical_cast<std::string>(cost_time.second);
            row->push_back(time_text, default_font, ClientUI::TextColor());

            // brief description
            std::string desc_text = UserString("BT_BUILDING");
            GG::Control* desc_control = new GG::TextControl(GG::X0, GG::Y0, desc_col_width, m_row_height, desc_text, default_font, ClientUI::TextColor(), GG::FORMAT_LEFT); ///< ctor taking a font directly
            row->push_back(desc_control);

            // is item buildable?  If not, disable row
            if (!empire->BuildableItem(BT_BUILDING, name, m_build_location)) {
                row->Disable(true);
            } else {
                row->Disable(false);
            }

            GG::ListBox::iterator row_it = m_buildable_items->Insert(row);
            m_build_types[row_it] = BT_BUILDING;
            if (row->DragDropDataType() == selected_row)
                row_to_select = row_it;

            row->GetLayout()->SetColumnStretch(0, 0.0);
            row->GetLayout()->SetColumnStretch(1, 0.0);
            row->GetLayout()->SetColumnStretch(2, 0.0);
            row->GetLayout()->SetColumnStretch(3, 0.0);
            row->GetLayout()->SetColumnStretch(4, 1.0);
        }
    }
    // populate with ship designs
    Logger().debugStream() << "Adding ship designs";
    if (m_build_types_shown.find(BT_SHIP) != m_build_types_shown.end()) {
        Empire::ShipDesignItr end_it = empire->ShipDesignEnd();
        for (Empire::ShipDesignItr it = empire->ShipDesignBegin(); it != end_it; ++it, ++i) {
            const int ship_design_id = *it;

            if (!BuildableItemVisible(BT_SHIP, ship_design_id)) continue;

            const ShipDesign* ship_design = GetShipDesign(ship_design_id);
            if (!ship_design) continue;

            GG::ListBox::Row* row = new GG::ListBox::Row();
            row->SetDragDropDataType(boost::lexical_cast<std::string>(ship_design_id));

            // icon
            GG::StaticGraphic* icon = new GG::StaticGraphic(GG::X0, GG::Y0, icon_col_width, m_row_height, 
                ClientUI::ShipIcon(ship_design->ID()),
                GG::GRAPHIC_FITGRAPHIC | GG::GRAPHIC_PROPSCALE);
            row->push_back(dynamic_cast<GG::Control*>(icon));

            // ship design name
            row->push_back(ship_design->Name(), default_font, ClientUI::TextColor());

            // cost / turn, and minimum production turns
            const std::pair<double, int> cost_time = empire->ProductionCostAndTime(BT_SHIP, ship_design_id);
            std::string cost_text = boost::lexical_cast<std::string>(cost_time.first);
            row->push_back(cost_text, default_font, ClientUI::TextColor());
            std::string time_text = boost::lexical_cast<std::string>(cost_time.second);
            row->push_back(time_text, default_font, ClientUI::TextColor());

            // brief description            
            std::string desc_text = UserString("BT_SHIP");
            GG::Control* desc_control = new GG::TextControl(GG::X0, GG::Y0, desc_col_width, m_row_height, desc_text, default_font, ClientUI::TextColor(), GG::FORMAT_LEFT); ///< ctor taking a font directly
            row->push_back(desc_control);

            // is item buildable?  If not, disable row
            if (!empire->BuildableItem(BT_SHIP, ship_design_id, m_build_location)) {
                row->Disable(true);
            } else {
                row->Disable(false);
            }

            GG::ListBox::iterator row_it = m_buildable_items->Insert(row);
            m_build_types[row_it] = BT_SHIP;
            if (row->DragDropDataType() == selected_row)
                row_to_select = row_it;

            row->GetLayout()->SetColumnStretch(0, 0.0);
            row->GetLayout()->SetColumnStretch(1, 0.0);
            row->GetLayout()->SetColumnStretch(2, 0.0);
            row->GetLayout()->SetColumnStretch(3, 0.0);
            row->GetLayout()->SetColumnStretch(4, 1.0);
        }
    }

    Logger().debugStream() << "Selecting Row";
    if (row_to_select != m_buildable_items->end())
        m_buildable_items->SelectRow(row_to_select);
    Logger().debugStream() << "Done";
}

std::vector<GG::X> BuildDesignatorWnd::BuildSelector::ColWidths()
{
    std::vector<GG::X> retval;

    retval.push_back(GG::X(Value(m_row_height)));  // icon
    retval.push_back(GG::X(ClientUI::Pts()*18));   // name
    retval.push_back(GG::X(ClientUI::Pts()*3));    // cost
    retval.push_back(GG::X(ClientUI::Pts()*2));    // time

    GG::X desc_col_width = m_buildable_items->ClientWidth() 
        - (retval[0] + retval[1] + retval[2] + retval[3])
        - ClientUI::ScrollWidth();
    retval.push_back(desc_col_width);

    return retval;
}

void BuildDesignatorWnd::BuildSelector::BuildItemSelected(const GG::ListBox::SelectionSet& selections)
{
    if (selections.size() == 1) {
        GG::ListBox::iterator row = *selections.begin();
        BuildType build_type = m_build_types[row];
        if (build_type == BT_BUILDING)
            DisplayNamedBuildItemSignal(BT_BUILDING, (*row)->DragDropDataType());
        else if (build_type == BT_SHIP)
            DisplayIDedBuildItemSignal(BT_SHIP, boost::lexical_cast<int>((*row)->DragDropDataType()));
    }
}

void BuildDesignatorWnd::BuildSelector::BuildItemDoubleClicked(GG::ListBox::iterator it)
{
    if ((*it)->Disabled())
        return;
    BuildType build_type = m_build_types[it];
    if (build_type == BT_BUILDING)
        RequestNamedBuildItemSignal(BT_BUILDING, (*it)->DragDropDataType(), 1);
    else if (build_type == BT_SHIP)
        RequestIDedBuildItemSignal(BT_SHIP, boost::lexical_cast<int>((*it)->DragDropDataType()), 1);
}


//////////////////////////////////////////////////
// BuildDesignatorWnd
//////////////////////////////////////////////////
BuildDesignatorWnd::BuildDesignatorWnd(GG::X w, GG::Y h) :
    Wnd(GG::X0, GG::Y0, w, h, GG::CLICKABLE | GG::ONTOP),
    m_build_location(UniverseObject::INVALID_OBJECT_ID)
{
    GG::X CHILD_WIDTHS = w - MapWnd::SIDE_PANEL_WIDTH;
    const GG::Y DETAIL_PANEL_HEIGHT = TechTreeWnd::NAVIGATOR_AND_DETAIL_HEIGHT;
    const GG::Y BUILD_SELECTOR_HEIGHT = DETAIL_PANEL_HEIGHT;

    m_build_detail_panel = new BuildDetailPanel(CHILD_WIDTHS, DETAIL_PANEL_HEIGHT);

    m_side_panel = new SidePanel(Width() - MapWnd::SIDE_PANEL_WIDTH, GG::Y0, MapWnd::SIDE_PANEL_WIDTH, GG::GUI::GetGUI()->AppHeight());
    m_side_panel->Hide();

    m_map_view_hole = GG::Rect(GG::X0, GG::Y0, CHILD_WIDTHS + SidePanel::MAX_PLANET_DIAMETER, h);

    m_build_selector = new BuildSelector(CHILD_WIDTHS, BUILD_SELECTOR_HEIGHT);
    m_build_selector->MoveTo(GG::Pt(GG::X0, h - BUILD_SELECTOR_HEIGHT));


    GG::Connect(m_build_selector->DisplayNamedBuildItemSignal, &BuildDesignatorWnd::BuildDetailPanel::SetBuildItem, m_build_detail_panel);
    GG::Connect(m_build_selector->DisplayIDedBuildItemSignal, &BuildDesignatorWnd::BuildDetailPanel::SetBuildItem, m_build_detail_panel);
    GG::Connect(m_build_selector->RequestNamedBuildItemSignal, &BuildDesignatorWnd::BuildItemRequested, this);
    GG::Connect(m_build_selector->RequestIDedBuildItemSignal, &BuildDesignatorWnd::BuildItemRequested, this);

    GG::Connect(m_side_panel->PlanetSelectedSignal, &BuildDesignatorWnd::SelectPlanet, this);
    GG::Connect(m_side_panel->SystemSelectedSignal, SystemSelectedSignal);


    // connect build type button clicks to update display
    GG::Connect(m_build_selector->m_build_type_buttons[BT_BUILDING]->ClickedSignal, ToggleBuildTypeFunctor(this, BT_BUILDING));
    GG::Connect(m_build_selector->m_build_type_buttons[BT_SHIP]->ClickedSignal, ToggleBuildTypeFunctor(this, BT_SHIP));
    GG::Connect(m_build_selector->m_build_type_buttons[NUM_BUILD_TYPES]->ClickedSignal, ToggleAllBuildTypesFunctor(this));    // last button should be "All" button

    // connect availability button clicks to update display
    GG::Connect(m_build_selector->m_availability_buttons.at(0)->ClickedSignal, ToggleAvailabilityFunctor(this, true));    // available items
    GG::Connect(m_build_selector->m_availability_buttons.at(1)->ClickedSignal, ToggleAvailabilityFunctor(this, false));   // UNavailable items

    AttachChild(m_build_detail_panel);
    AttachChild(m_build_selector);
    AttachChild(m_side_panel);

    MoveChildUp(m_build_detail_panel);
    MoveChildUp(m_build_selector);

    ShowAllTypes(false);            // show all types without populating the list
    HideAvailability(false, false); // hide unavailable items without populating the list
    ShowAvailability(true, false);  // show available items without populating the list
}

const std::set<BuildType>& BuildDesignatorWnd::GetBuildTypesShown() const
{
    return m_build_selector->GetBuildTypesShown();
}

const std::pair<bool, bool>& BuildDesignatorWnd::GetAvailabilitiesShown() const
{
    return m_build_selector->GetAvailabilitiesShown();
}

bool BuildDesignatorWnd::InWindow(const GG::Pt& pt) const
{
    GG::Rect clip_rect = m_map_view_hole + UpperLeft();
    return clip_rect.Contains(pt) ?
        (m_build_detail_panel->InWindow(pt) || m_build_selector->InWindow(pt) || m_side_panel->InWindow(pt)) :
        Wnd::InClient(pt);
}

bool BuildDesignatorWnd::InClient(const GG::Pt& pt) const
{
    GG::Rect clip_rect = m_map_view_hole + UpperLeft();
    return clip_rect.Contains(pt) ?
        (m_build_detail_panel->InClient(pt) || m_build_selector->InClient(pt) || m_side_panel->InClient(pt)) :
        Wnd::InClient(pt);
}

GG::Rect BuildDesignatorWnd::MapViewHole() const
{
    return m_map_view_hole;
}

void BuildDesignatorWnd::CenterOnBuild(int queue_idx)
{
    m_build_detail_panel->SetBuild(queue_idx);
    Empire* empire = Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
    const ProductionQueue& queue = empire->GetProductionQueue();
    if (0 <= queue_idx && queue_idx < static_cast<int>(queue.size())) {
        UniverseObject* build_location = GetUniverse().Object(queue[queue_idx].location);
        assert(build_location);
        // this code assumes that the build site is a planet
        int system = build_location->SystemID();
        MapWnd* map = ClientUI::GetClientUI()->GetMapWnd();
        map->CenterOnObject(system);
        if (m_side_panel->SystemID() != system)
            SystemSelectedSignal(system);
        m_side_panel->SelectPlanet(queue[queue_idx].location);
    }
}

void BuildDesignatorWnd::SelectSystem(int system)
{
    if (system != UniverseObject::INVALID_OBJECT_ID) {
        if (system != m_side_panel->SystemID()) {
            m_build_location = UniverseObject::INVALID_OBJECT_ID;
        }
        m_side_panel->SetSystem(system);
        SelectDefaultPlanet(system);
    }
}

void BuildDesignatorWnd::SelectPlanet(int planet)
{
    m_build_location = planet;
    m_build_selector->SetBuildLocation(planet);
    if (planet != UniverseObject::INVALID_OBJECT_ID)
        m_system_default_planets[m_side_panel->SystemID()] = planet;
}

void BuildDesignatorWnd::Reset()
{
    // default to the home system when nothing is selected in the main map's SidePanel
    if (m_side_panel->SystemID() == UniverseObject::INVALID_OBJECT_ID) {
        int home_system_id = GetUniverse().Object<Planet>(Empires().Lookup(HumanClientApp::GetApp()->EmpireID())->HomeworldID())->SystemID();
        SystemSelectedSignal(home_system_id);
    }
    SelectDefaultPlanet(m_side_panel->SystemID());
    m_build_selector->Reset();
    m_build_detail_panel->SetBuildItem(INVALID_BUILD_TYPE);
    m_side_panel->Refresh();
}

void BuildDesignatorWnd::Clear()
{
    m_build_detail_panel->SetBuildItem(INVALID_BUILD_TYPE);
    m_build_selector->Reset();
    SystemSelectedSignal(UniverseObject::INVALID_OBJECT_ID);
    m_side_panel->Hide();
    m_build_location = UniverseObject::INVALID_OBJECT_ID;
    m_system_default_planets.clear();
}

void BuildDesignatorWnd::ShowType(BuildType type, bool refresh_list)
{
    Logger().errorStream() << "BuildDesignatorWnd::ShowType(" << boost::lexical_cast<std::string>(type) << ")";
    if (type == BT_BUILDING || type == BT_SHIP) {
        m_build_selector->ShowType(type, refresh_list);
        m_build_selector->m_build_type_buttons[type]->MarkSelectedGray();
    } else {
        throw std::invalid_argument("BuildDesignatorWnd::ShowType was passed an invalid BuildType");
    }
}

void BuildDesignatorWnd::ShowAllTypes(bool refresh_list)
{
    m_build_selector->ShowAllTypes(refresh_list);
    m_build_selector->m_build_type_buttons[BT_BUILDING]->MarkSelectedGray();
    m_build_selector->m_build_type_buttons[BT_SHIP]->MarkSelectedGray();
}

void BuildDesignatorWnd::HideType(BuildType type, bool refresh_list)
{
    Logger().errorStream() << "BuildDesignatorWnd::HideType(" << boost::lexical_cast<std::string>(type) << ")";
    if (type == BT_BUILDING || type == BT_SHIP) {
        m_build_selector->HideType(type, refresh_list);
        m_build_selector->m_build_type_buttons[type]->MarkNotSelected();
    } else {
        throw std::invalid_argument("BuildDesignatorWnd::HideType was passed an invalid BuildType");
    }
}

void BuildDesignatorWnd::HideAllTypes(bool refresh_list)
{
    m_build_selector->HideAllTypes(refresh_list);
    m_build_selector->m_build_type_buttons[BT_BUILDING]->MarkNotSelected();
    m_build_selector->m_build_type_buttons[BT_SHIP]->MarkNotSelected();
}

void BuildDesignatorWnd::ToggleType(BuildType type, bool refresh_list)
{
    if (type == BT_BUILDING || type == BT_SHIP) {
        const std::set<BuildType>& types_shown = m_build_selector->GetBuildTypesShown();
        if (types_shown.find(type) == types_shown.end())
            ShowType(type, refresh_list);
        else
            HideType(type, refresh_list);
    } else {
        throw std::invalid_argument("BuildDesignatorWnd::ShowType was passed an invalid BuildType");
    } 
}

void BuildDesignatorWnd::ToggleAllTypes(bool refresh_list)
{
    const std::set<BuildType>& types_shown = m_build_selector->GetBuildTypesShown();
    if (types_shown.size() == NUM_BUILD_TYPES - 1)  // -1 because there are no buttons for BuildType::BT_NOT_BUILDING
        HideAllTypes(refresh_list);
    else
        ShowAllTypes(refresh_list);
}

void BuildDesignatorWnd::ShowAvailability(bool available, bool refresh_list)
{
    m_build_selector->ShowAvailability(available, refresh_list);
    if (available)
        m_build_selector->m_availability_buttons.at(0)->MarkSelectedGray();
    else
        m_build_selector->m_availability_buttons.at(1)->MarkSelectedGray();
}

void BuildDesignatorWnd::HideAvailability(bool available, bool refresh_list)
{
    m_build_selector->HideAvailability(available, refresh_list);
    if (available)
        m_build_selector->m_availability_buttons.at(0)->MarkNotSelected();
    else
        m_build_selector->m_availability_buttons.at(1)->MarkNotSelected();
}

void BuildDesignatorWnd::ToggleAvailabilitly(bool available, bool refresh_list)
{
    const std::pair<bool, bool>& avail_shown = m_build_selector->GetAvailabilitiesShown();
    if (available) {
        if (avail_shown.first)
            HideAvailability(true, refresh_list);
        else
            ShowAvailability(true, refresh_list);
    } else {
        if (avail_shown.second)
            HideAvailability(false, refresh_list);
        else
            ShowAvailability(false, refresh_list);
    }
}

void BuildDesignatorWnd::BuildItemRequested(BuildType build_type, const std::string& item, int num_to_build)
{
    Empire* empire = Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
    if (empire && empire->BuildableItem(build_type, item, m_build_location))
        AddNamedBuildToQueueSignal(build_type, item, num_to_build, m_build_location);
}

void BuildDesignatorWnd::BuildItemRequested(BuildType build_type, int design_id, int num_to_build)
{
    Empire* empire = Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
    if (empire && empire->BuildableItem(build_type, design_id, m_build_location))
        AddIDedBuildToQueueSignal(build_type, design_id, num_to_build, m_build_location);
}

void BuildDesignatorWnd::BuildQuantityChanged(int queue_idx, int quantity)
{
    BuildQuantityChangedSignal(queue_idx, quantity);
}

void BuildDesignatorWnd::SelectDefaultPlanet(int system)
{
    m_side_panel->SetValidSelectionPredicate(boost::shared_ptr<UniverseObjectVisitor>(new OwnedVisitor<Planet>(HumanClientApp::GetApp()->EmpireID())));
    std::map<int, int>::iterator it = m_system_default_planets.find(system);
    if (it != m_system_default_planets.end()) {
        // if a planet has previously been selected in this system, re-select it
        m_side_panel->SelectPlanet(it->second);
    } else {
        // find a planet to select from those owned by this client's player
        const System* sys = GetUniverse().Object<System>(system);
        if (!sys) {
            Logger().errorStream() << "BuildDesignatorWnd::SelectDefaultPlanet couldn't get system with id " << system;
            return;
        }

        int empire_id = HumanClientApp::GetApp()->EmpireID();
        System::ConstObjectVec owned_planets = sys->FindObjects(OwnedVisitor<Planet>(empire_id));

        if (!owned_planets.empty()) {
            // pick planet with max population of those owned by this player in this system
            int planet_id = owned_planets[0]->ID();
            double max_pop = owned_planets[0]->GetMeter(METER_POPULATION)->Current();
            for (unsigned int i = 1; i < owned_planets.size(); ++i) {
                double pop = owned_planets[0]->GetMeter(METER_POPULATION)->Current();
                if (max_pop < pop) {
                    max_pop = pop;
                    planet_id = owned_planets[0]->ID();
                }
            }
            m_side_panel->SelectPlanet(planet_id);
        }
    }
}
