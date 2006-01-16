// -*- C++ -*-
#ifndef _FleetWindow_h_
#define _FleetWindow_h_

#ifndef _CUIWnd_h_
#include "CUIWnd.h"
#endif

#ifndef _GG_ListBox_h_
#include <GG/ListBox.h>
#endif

#ifndef _MapWnd_h_
#include "MapWnd.h"
#endif

class CUIButton;
class CUIListBox;
class Fleet;
class Ship;
class System;
class UniverseObject;
namespace GG {
class TextControl;
}


class FleetsWnd;

class FleetDetailPanel : public GG::Wnd
{
public:
    /** \name Signal Types */ //@{
    typedef boost::signal<void (Fleet*)>      PanelEmptySignalType;    ///< emitted when the panel is empty (no ships)
    typedef boost::signal<Fleet* (int)>       NeedNewFleetSignalType;  ///< emitted when ships are dragged and dropped into a null fleet
    //@}

    /** \name Slot Types */ //@{
    typedef PanelEmptySignalType::slot_type   PanelEmptySlotType;      ///< type of functor(s) invoked on a PanelEmptySignalType
    typedef NeedNewFleetSignalType::slot_type NeedNewFleetSlotType;    ///< type of functor(s) invoked on a CreateNewFleetSignalType
    //@}

    /** \name Structors */ //@{
    FleetDetailPanel(int x, int y, Fleet* fleet, bool read_only, Uint32 flags = 0); ///< ctor
    virtual ~FleetDetailPanel();
    //@}

    /** \name Accessors */ //@{
    int          GetShipIDOfListRow(int row_idx) const; ///< returns the ID number of the ship in row \a row_idx of the ships listbox
    Fleet*       GetFleet() const {return m_fleet;} ///< returns the currently-displayed fleet (may be 0)

    mutable PanelEmptySignalType   PanelEmptySignal;
    mutable NeedNewFleetSignalType NeedNewFleetSignal;
    //@}

    //! \name Mutators //@{
    void SetFleet(Fleet* fleet); ///< sets the currently-displayed Fleet (may be null)
    //@}

protected:
    //! \name Mutators //@{
    virtual void CloseClicked();
    //@}

private:
    void        Init();
    void        AttachSignalChildren();
    void        DetachSignalChildren();
    void        Refresh();
    void        UniverseObjectDelete(const UniverseObject *);

    void        ShipSelectionChanged(const std::set<int>& rows);
    void        ShipBrowsed(int row_idx);
    void        ShipDroppedIntoList(int row_idx, GG::ListBox::Row* row);
    void        ShipRightClicked(int row_idx, GG::ListBox::Row* row, const GG::Pt& pt);
    std::string DestinationText() const;
    std::string ShipStatusText(int ship_id) const;

    Fleet*                      m_fleet;
    const bool                  m_read_only;
    boost::signals::connection  m_fleet_connection;
    boost::signals::connection  m_universe_object_delete_connection;

    GG::TextControl*            m_destination_text;
    CUIListBox*                 m_ships_lb;
    GG::TextControl*            m_ship_status_text;
};

class FleetDetailWnd : public CUIWnd
{
public:
    /** \name Signal Types */ //@{
    typedef boost::signal<void (FleetDetailWnd*)>        ClosingSignalType;      ///< emitted when this window is about to close
    typedef boost::signal<Fleet* (FleetDetailWnd*, int)> NeedNewFleetSignalType; ///< emitted when ships are dragged and dropped into a null fleet
    //@}

    /** \name Slot Types */ //@{
    typedef ClosingSignalType::slot_type                 ClosingSlotType;      ///< type of functor(s) invoked on a ClosingSignalType
    typedef NeedNewFleetSignalType::slot_type            NeedNewFleetSlotType; ///< type of functor(s) invoked on a NeedNewFleetSignalType
    //@}

    /** \name Structors */ //@{
    FleetDetailWnd(int x, int y, Fleet* fleet, bool read_only, Uint32 flags = GG::CLICKABLE | GG::DRAGABLE | GG::ONTOP | CLOSABLE | MINIMIZABLE); ///< basic ctor
    ~FleetDetailWnd(); ///< dtor
    //@}

    //! \name Accessors //@{
    FleetDetailPanel&       GetFleetDetailPanel() const {return *m_fleet_panel;} ///< returns the internally-held fleet panel for theis window

    mutable NeedNewFleetSignalType NeedNewFleetSignal;
    mutable ClosingSignalType      ClosingSignal;
    //@}

protected:
    //! \name Mutators //@{
    virtual void CloseClicked();
    //@}

private:
    Fleet*      PanelNeedsNewFleet(int ship_id) {return NeedNewFleetSignal(this, ship_id);}
    void        AttachSignalChildren();
    void        DetachSignalChildren();
    std::string TitleText() const;

    FleetDetailPanel*  m_fleet_panel;
};


class FleetWnd : public MapWndPopup
{
public:
    /** \name Signal Types */ //@{
    typedef boost::signal<void (Fleet*, FleetWnd*)> ShowingFleetSignalType;    ///< emitted to indicate to the rest of the UI that this window is showing the given fleet, so duplicates are avoided
    typedef boost::signal<void (Fleet*)>            NotShowingFleetSignalType; ///< emitted to indicate that this window is not showing the given fleet
    //@}

    /** \name Slot Types */ //@{
    typedef ShowingFleetSignalType::slot_type       ShowingFleetSlotType;      ///< type of functor(s) invoked on a ShowingFleetSignalType
    typedef NotShowingFleetSignalType::slot_type    NotShowingFleetSlotType;   ///< type of functor(s) invoked on a NotShowingFleetSignalType
    //@}

    /** \name Structors */ //@{
    /** constructs a fleet window for fleets in transit between systems */
    FleetWnd(int x, int y, std::vector<Fleet*> fleets, int selected_fleet, bool read_only, Uint32 flags = GG::CLICKABLE | GG::DRAGABLE | GG::ONTOP | CLOSABLE | MINIMIZABLE);
    ~FleetWnd(); ///< dtor
    //@}

    //! \name Mutators //@{
    mutable ShowingFleetSignalType    ShowingFleetSignal;
    mutable NotShowingFleetSignalType NotShowingFleetSignal;
    //@}

    //! \name Mutators //@{
    void SystemClicked(int system_id); ///< invoked when a system is clicked on the main map, possibly indicating that the currently-selected fleet should move there
    void AddFleet(Fleet* fleet); ///< adds a new fleet to a currently-open FletWnd
    void SelectFleet(Fleet* fleet); ///< selects the indicated fleet, bringing it into the fleet detail window
    //@}
    
    int SystemID() const {return m_system_id;}
    bool ContainsFleet(int fleet_id) const;

    static bool FleetWndsOpen();     ///< returns true iff one or more fleet windows are open
    static bool CloseAllFleetWnds(); ///< returns true iff fleet windows were open before it was called.  Used most often for fleet window quick-close.
    static GG::Pt LastPosition();    ///< returns the last position of the last FleetWnd that was closed
    
    typedef std::set<FleetWnd*>::const_iterator FleetWndItr;
    static FleetWndItr FleetWndBegin();
    static FleetWndItr FleetWndEnd  ();

protected:
    //! \name Mutators //@{
    virtual void CloseClicked();
    //@}

private:
    void        Init(const std::vector<Fleet*>& fleet_ids, int selected_fleet);
    void        AttachSignalChildren();
    void        DetachSignalChildren();
    void        FleetBrowsed(int row_idx);
    void        FleetSelectionChanged(const std::set<int>& rows);
    void        FleetRightClicked(int row_idx, GG::ListBox::Row* row, const GG::Pt& pt);
    void        FleetDoubleClicked(int row_idx, GG::ListBox::Row* row);
    void        FleetDeleted(int row_idx, GG::ListBox::Row* row);
    void        ObjectDroppedIntoList(int row_idx, GG::ListBox::Row* row);
    void        NewFleetButtonClicked();
    void        FleetDetailWndClosing(FleetDetailWnd* wnd);
    Fleet*      FleetInRow(int idx) const;
    std::string TitleText() const;
    void        FleetPanelEmpty(Fleet* fleet);
    void        DeleteFleet(Fleet* fleet);
    Fleet*      CreateNewFleetFromDrop(int ship_id);
    void        RemoveEmptyFleets();
    void        UniverseObjectDelete(const UniverseObject *);
    void        SystemChangedSlot();

    const int           m_empire_id;
    int                 m_system_id;
    const bool          m_read_only;
    bool                m_moving_fleets;

    int                 m_current_fleet;

    std::map<Fleet*, FleetDetailWnd*> m_open_fleet_windows;
    std::set<FleetDetailWnd*>         m_new_fleet_windows;

    CUIListBox*         m_fleets_lb;
    FleetDetailPanel*   m_fleet_detail_panel;

    boost::signals::connection  m_universe_object_delete_connection;
    boost::signals::connection  m_lb_delete_connection;
    boost::signals::connection  m_system_changed_connection;

    static std::set<FleetWnd*> s_open_fleet_wnds;
    static GG::Pt s_last_position; ///< the latest position to which any FleetWnd has been moved.  This is used to keep the place of the fleet window in single-fleetwindow mode.
};

inline std::pair<std::string, std::string> FleetWindowRevision()
{return std::pair<std::string, std::string>("$RCSfile$", "$Revision$");}

#endif // _FleetWindow_h_

