#include "ResearchWnd.h"

#include "ClientUI.h"
#include "CUIControls.h"
#include "QueueListBox.h"
#include "../Empire/Empire.h"
#include "../universe/Tech.h"
#include "../util/MultiplayerCommon.h"
#include "../util/AppInterface.h"
#include "../UI/TechTreeWnd.h"
#include "../client/human/HumanClientApp.h"

#include <GG/DrawUtil.h>
#include <GG/StaticGraphic.h>

#include <boost/cast.hpp>

#include <cmath>

namespace {
    const GG::X RESEARCH_INFO_AND_QUEUE_WIDTH(250);
    const float OUTER_LINE_THICKNESS = 2.0f;

    //////////////////////////////////////////////////
    // QueueRow
    //////////////////////////////////////////////////
    struct QueueRow : GG::ListBox::Row {
        QueueRow(GG::X w, const ResearchQueue::Element& queue_element);
        std::string tech_name;
    };

    //////////////////////////////////////////////////
    // QueueTechPanel
    //////////////////////////////////////////////////
    class QueueTechPanel : public GG::Control {
    public:
        QueueTechPanel(GG::X w, const std::string& tech_name, double allocated_rp,
                       int turns_left, double turns_completed);
        virtual void Render();

    private:
        void Draw(GG::Clr clr, bool fill);

        const std::string&      m_tech_name;
        GG::TextControl*        m_name_text;
        GG::TextControl*        m_RPs_and_turns_text;
        GG::TextControl*        m_turns_remaining_text;
        GG::StaticGraphic*      m_icon;
        MultiTurnProgressBar*   m_progress_bar;
        bool                    m_in_progress;
        int                     m_total_turns;
        double                  m_turns_completed;
    };

    //////////////////////////////////////////////////
    // QueueRow implementation
    //////////////////////////////////////////////////
    QueueRow::QueueRow(GG::X w, const ResearchQueue::Element& queue_element) :
        GG::ListBox::Row(),
        tech_name(queue_element.name)
    {
        int empire_id = HumanClientApp::GetApp()->EmpireID();
        const Empire* empire = Empires().Lookup(empire_id);

        const Tech* tech = GetTech(tech_name);
        double per_turn_cost = tech ? tech->PerTurnCost(empire_id) : 1;
        double progress = 0.0;
        if (empire && empire->TechResearched(tech_name))
            progress = tech->ResearchCost(empire_id);
        else if (empire)
            progress = empire->ResearchProgress(tech_name);

        GG::Control* panel = new QueueTechPanel(w, tech_name, queue_element.allocated_rp,
                                                queue_element.turns_left, progress / per_turn_cost);
        Resize(panel->Size());
        push_back(panel);

        SetDragDropDataType("RESEARCH_QUEUE_ROW");
    }

    //////////////////////////////////////////////////
    // QueueTechPanel implementation
    //////////////////////////////////////////////////
    QueueTechPanel::QueueTechPanel(GG::X w, const std::string& tech_name, double turn_spending,
                                   int turns_left, double turns_completed) :
        GG::Control(GG::X0, GG::Y0, w, GG::Y(10), GG::Flags<GG::WndFlag>()),
        m_tech_name(tech_name),
        m_in_progress(turn_spending),
        m_total_turns(1),
        m_turns_completed(turns_completed)
    {
        const int MARGIN = 2;

        const int FONT_PTS = ClientUI::Pts();
        const GG::Y METER_HEIGHT(FONT_PTS);

        const GG::Y HEIGHT = MARGIN + FONT_PTS + MARGIN + METER_HEIGHT + MARGIN + FONT_PTS + MARGIN + 6;

        const int GRAPHIC_SIZE = Value(HEIGHT - 9);    // 9 pixels accounts for border thickness so the sharp-cornered icon doesn't with the rounded panel corner

        const GG::X NAME_WIDTH = w - GRAPHIC_SIZE - 2*MARGIN - 3;
        const GG::X METER_WIDTH = w - GRAPHIC_SIZE - 3*MARGIN - 3;
        const GG::X TURNS_AND_COST_WIDTH = NAME_WIDTH/2;

        const Tech* tech = GetTech(m_tech_name);
        if (tech)
            m_total_turns = tech->ResearchTime(HumanClientApp::GetApp()->EmpireID());

        Resize(GG::Pt(w, HEIGHT));

        GG::Clr clr = m_in_progress ? GG::LightColor(ClientUI::ResearchableTechTextAndBorderColor()) : ClientUI::ResearchableTechTextAndBorderColor();
        boost::shared_ptr<GG::Font> font = ClientUI::GetFont();

        GG::Y top(MARGIN);
        GG::X left(MARGIN);


        m_icon = new GG::StaticGraphic(left, top, GG::X(GRAPHIC_SIZE), GG::Y(GRAPHIC_SIZE),
                                       ClientUI::TechIcon(m_tech_name),
                                       GG::GRAPHIC_FITGRAPHIC);
        m_icon->SetColor(tech ? ClientUI::CategoryColor(tech->Category()) : GG::Clr());

        left += m_icon->Width() + MARGIN;

        m_name_text = new GG::TextControl(left, top, NAME_WIDTH, GG::Y(FONT_PTS + 2*MARGIN),
                                          UserString(m_tech_name),
                                          font, clr, GG::FORMAT_TOP | GG::FORMAT_LEFT);
        m_name_text->ClipText(true);

        top += m_name_text->Height();    // not sure why I need two margins here... otherwise the progress bar appears over the bottom of the text

        m_progress_bar = new MultiTurnProgressBar(METER_WIDTH, METER_HEIGHT, tech ? tech->ResearchTime(HumanClientApp::GetApp()->EmpireID()) : 1,
                                                  turns_completed,
                                                  GG::LightColor(ClientUI::TechWndProgressBarBackgroundColor()),
                                                  ClientUI::TechWndProgressBarColor(),
                                                  m_in_progress ? ClientUI::ResearchableTechFillColor() : GG::LightColor(ClientUI::ResearchableTechFillColor()) );
        m_progress_bar->MoveTo(GG::Pt(left, top));

        top += m_progress_bar->Height() + MARGIN;

        using boost::io::str;

        std::string turns_cost_text = str(FlexibleFormat(UserString("TECH_TURN_COST_STR")) % DoubleToString(turn_spending, 3, false));
        m_RPs_and_turns_text = new GG::TextControl(left, top, TURNS_AND_COST_WIDTH, GG::Y(FONT_PTS + MARGIN),
                                                   turns_cost_text, font, clr, GG::FORMAT_LEFT);

        left += TURNS_AND_COST_WIDTH;

        std::string turns_left_text = turns_left < 0 ? UserString("TECH_TURNS_LEFT_NEVER")
                                                     : str(FlexibleFormat(UserString("TECH_TURNS_LEFT_STR")) % turns_left);
        m_turns_remaining_text = new GG::TextControl(left, top, TURNS_AND_COST_WIDTH, GG::Y(FONT_PTS + MARGIN),
                                                     turns_left_text, font, clr, GG::FORMAT_RIGHT);
        m_turns_remaining_text->ClipText(true);


        AttachChild(m_name_text);
        AttachChild(m_RPs_and_turns_text);
        AttachChild(m_turns_remaining_text);
        AttachChild(m_icon);
        AttachChild(m_progress_bar);
    }

    void QueueTechPanel::Render() {
        GG::Clr fill = m_in_progress ? GG::LightColor(ClientUI::ResearchableTechFillColor()) : ClientUI::ResearchableTechFillColor();
        GG::Clr text_and_border = m_in_progress ? GG::LightColor(ClientUI::ResearchableTechTextAndBorderColor()) : ClientUI::ResearchableTechTextAndBorderColor();

        glDisable(GL_TEXTURE_2D);
        Draw(fill, true);
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(static_cast<GLfloat>(OUTER_LINE_THICKNESS));
        Draw(GG::Clr(text_and_border.r, text_and_border.g, text_and_border.b, 127), false);
        glLineWidth(1.0);
        glDisable(GL_LINE_SMOOTH);
        Draw(GG::Clr(text_and_border.r, text_and_border.g, text_and_border.b, 255), false);
        glEnable(GL_TEXTURE_2D);
    }

    void QueueTechPanel::Draw(GG::Clr clr, bool fill) {
        const int CORNER_RADIUS = 7;
        glColor(clr);
        PartlyRoundedRect(UpperLeft(), LowerRight(), CORNER_RADIUS, true, false, true, false, fill);
    }
}


//////////////////////////////////////////////////
// ResearchWnd                                  //
//////////////////////////////////////////////////
ResearchWnd::ResearchWnd(GG::X w, GG::Y h) :
    GG::Wnd(GG::X0, GG::Y0, w, h, GG::INTERACTIVE | GG::ONTOP),
    m_research_info_panel(0),
    m_queue_lb(0),
    m_tech_tree_wnd(0),
    m_enabled(false)
{
    m_research_info_panel = new ProductionInfoPanel(RESEARCH_INFO_AND_QUEUE_WIDTH, GG::Y(200), UserString("RESEARCH_INFO_PANEL_TITLE"), UserString("RESEARCH_INFO_RP"),
                                                    OUTER_LINE_THICKNESS, ClientUI::KnownTechFillColor(), ClientUI::KnownTechTextAndBorderColor());

    m_queue_lb = new QueueListBox(GG::X(2), m_research_info_panel->LowerRight().y,
                                  m_research_info_panel->Width() - 4, ClientSize().y - 4 - m_research_info_panel->Height(),
                                  "RESEARCH_QUEUE_ROW");
    GG::Connect(m_queue_lb->QueueItemMoved, &ResearchWnd::QueueItemMoved, this);
    m_queue_lb->SetStyle(GG::LIST_NOSORT | GG::LIST_NOSEL | GG::LIST_USERDELETE);

    GG::Pt tech_tree_wnd_size = ClientSize() - GG::Pt(m_research_info_panel->Width(), GG::Y0);
    m_tech_tree_wnd = new TechTreeWnd(tech_tree_wnd_size.x, tech_tree_wnd_size.y);
    m_tech_tree_wnd->MoveTo(GG::Pt(m_research_info_panel->Width(), GG::Y0));

    GG::Connect(m_tech_tree_wnd->AddTechToQueueSignal,          &ResearchWnd::AddTechToQueueSlot,           this);
    GG::Connect(m_tech_tree_wnd->AddMultipleTechsToQueueSignal, &ResearchWnd::AddMultipleTechsToQueueSlot,  this);
    GG::Connect(m_queue_lb->ErasedSignal,                       &ResearchWnd::QueueItemDeletedSlot,         this);
    GG::Connect(m_queue_lb->LeftClickedSignal,                  &ResearchWnd::QueueItemClickedSlot,         this);
    GG::Connect(m_queue_lb->DoubleClickedSignal,                &ResearchWnd::QueueItemDoubleClickedSlot,   this);

    AttachChild(m_research_info_panel);
    AttachChild(m_queue_lb);
    AttachChild(m_tech_tree_wnd);

    SetChildClippingMode(ClipToClient);
}

ResearchWnd::~ResearchWnd()
{ m_empire_connection.disconnect(); }

void ResearchWnd::Refresh() {
    // useful at start of turn or when loading empire from save.
    // since empire object is recreated based on turn update from server, 
    // connections of signals emitted from the empire must be remade
    m_empire_connection.disconnect();
    EmpireManager& manager = HumanClientApp::GetApp()->Empires();
    if (Empire* empire = manager.Lookup(HumanClientApp::GetApp()->EmpireID()))
        m_empire_connection = GG::Connect(empire->GetResearchQueue().ResearchQueueChangedSignal,
                                          &ResearchWnd::ResearchQueueChangedSlot, this);
    Update();
}

void ResearchWnd::Reset() {
    m_tech_tree_wnd->Reset();
    UpdateQueue();
    UpdateInfoPanel();
    m_queue_lb->BringRowIntoView(m_queue_lb->begin());
}

void ResearchWnd::Update() {
    m_tech_tree_wnd->Update();
    UpdateQueue();
    UpdateInfoPanel();
}

void ResearchWnd::CenterOnTech(const std::string& tech_name)
{ m_tech_tree_wnd->CenterOnTech(tech_name); }

void ResearchWnd::ShowTech(const std::string& tech_name) {
    m_tech_tree_wnd->CenterOnTech(tech_name);
    m_tech_tree_wnd->SetEncyclopediaTech(tech_name);
    m_tech_tree_wnd->SelectTech(tech_name);
}

void ResearchWnd::QueueItemMoved(GG::ListBox::Row* row, std::size_t position) {
    if (QueueRow* queue_row = boost::polymorphic_downcast<QueueRow*>(row))
        HumanClientApp::GetApp()->Orders().IssueOrder(
            OrderPtr(new ResearchQueueOrder(HumanClientApp::GetApp()->EmpireID(),
                                            queue_row->tech_name, position)));
}

void ResearchWnd::Sanitize()
{ m_tech_tree_wnd->Clear(); }

void ResearchWnd::Render()
{}

void ResearchWnd::ResearchQueueChangedSlot() {
    UpdateQueue();
    UpdateInfoPanel();
    m_tech_tree_wnd->Update();
}

void ResearchWnd::UpdateQueue() {
    const Empire* empire = Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
    if (!empire)
        return;

    const ResearchQueue& queue = empire->GetResearchQueue();
    std::size_t first_visible_queue_row = std::distance(m_queue_lb->begin(), m_queue_lb->FirstRowShown());
    m_queue_lb->Clear();
    const GG::X QUEUE_WIDTH = m_queue_lb->Width() - 8 - 14;

    for (ResearchQueue::const_iterator it = queue.begin(); it != queue.end(); ++it)
        m_queue_lb->Insert(new QueueRow(QUEUE_WIDTH, *it));

    if (!m_queue_lb->Empty())
        m_queue_lb->BringRowIntoView(--m_queue_lb->end());
    if (first_visible_queue_row < m_queue_lb->NumRows())
        m_queue_lb->BringRowIntoView(boost::next(m_queue_lb->begin(), first_visible_queue_row));
}

void ResearchWnd::UpdateInfoPanel() {
    const Empire* empire = Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
    if (!empire)
        return;
    const ResearchQueue& queue = empire->GetResearchQueue();
    double RPs = empire->ResourceProduction(RE_RESEARCH);
    double total_queue_cost = queue.TotalRPsSpent();
    ResearchQueue::const_iterator underfunded_it = queue.UnderfundedProject();
    double RPs_to_underfunded_projects = underfunded_it == queue.end() ? 0.0 : underfunded_it->allocated_rp;
    m_research_info_panel->Reset(RPs, total_queue_cost, queue.ProjectsInProgress(), RPs_to_underfunded_projects, queue.size());
    /* Altering research queue may have freed up or required more RP.  Signalling that the
       ResearchResPool has changed causes the MapWnd to be signalled that that pool has changed,
       which causes the resource indicator to be updated (which polls the ResearchQueue to
       determine how many RPs are being spent).  If/when RP are stockpilable, this might matter,
       so then the following line should be uncommented.*/
    //empire->GetResearchResPool().ChangedSignal();
}

void ResearchWnd::AddTechToQueueSlot(const std::string& tech_name) {
    if (!m_enabled)
        return;
    int empire_id = HumanClientApp::GetApp()->EmpireID();
    const Empire* empire = Empires().Lookup(empire_id);
    if (!empire)
        return;
    const ResearchQueue& queue = empire->GetResearchQueue();
    OrderSet& orders = HumanClientApp::GetApp()->Orders();
    if (!queue.InQueue(tech_name))
        orders.IssueOrder(OrderPtr(new ResearchQueueOrder(empire_id, tech_name, -1)));
}

void ResearchWnd::AddMultipleTechsToQueueSlot(const std::vector<std::string>& tech_vec) {
    if (!m_enabled)
        return;
    int empire_id = HumanClientApp::GetApp()->EmpireID();
    const Empire* empire = Empires().Lookup(empire_id);
    if (!empire)
        return;
    const ResearchQueue& queue = empire->GetResearchQueue();
    OrderSet& orders = HumanClientApp::GetApp()->Orders();
    for (std::vector<std::string>::const_iterator it = tech_vec.begin(); it != tech_vec.end(); ++it) {
        const std::string& tech_name = *it;
        if (!queue.InQueue(tech_name))
            orders.IssueOrder(OrderPtr(new ResearchQueueOrder(empire_id, tech_name, -1)));
    }
}

void ResearchWnd::QueueItemDeletedSlot(GG::ListBox::iterator it) {
    if (!m_enabled)
        return;
    int empire_id = HumanClientApp::GetApp()->EmpireID();
    OrderSet& orders = HumanClientApp::GetApp()->Orders();
    if (QueueRow* queue_row = boost::polymorphic_downcast<QueueRow*>(*it))
        orders.IssueOrder(OrderPtr(new ResearchQueueOrder(empire_id, queue_row->tech_name)));
}

void ResearchWnd::QueueItemClickedSlot(GG::ListBox::iterator it, const GG::Pt& pt) {
    QueueRow* queue_row = boost::polymorphic_downcast<QueueRow*>(*it);
    if (!queue_row)
        return;
    ShowTech(queue_row->tech_name);
}

void ResearchWnd::QueueItemDoubleClickedSlot(GG::ListBox::iterator it) {
    if (m_enabled)
        m_queue_lb->ErasedSignal(it);
}

void ResearchWnd::EnableOrderIssuing(bool enable/* = true*/) {
    m_enabled = enable;
    m_queue_lb->EnableOrderIssuing(m_enabled);
}
