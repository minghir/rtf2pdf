#pragma once
#include "Style.hpp" // Include clasa Style
#include "Page.hpp"  // Include clasa Page (sau referinta la ea)
#include "xhtml.hpp"




struct BoxMetrics {
    // 1. DIMENSIUNI BOX (Box Model Total)
    double width;       // style.width (Lățimea totală a box-ului, inclusiv padding și border)
    double height;      // style.height (Înălțimea totală a box-ului, inclusiv padding și border)

    // 2. COORDONATE RANDARE BOX (Marginea Bordurii) - În sistemul Layout (Top-Down)
    double x_start;     // X-ul marginii stângi a bordurii
    double y_start;     // Y-ul marginii superioare a bordurii (Y mai mare = Sus)
    double x_end;       // X-ul marginii drepte a bordurii (x_start + width)
    double y_end;       // Y-ul marginii inferioare a bordurii (y_start - height) // <-- CRUCIAL: Se scade!

    // 3. DIMENSIUNI CONȚINUT (Spațiul pentru text/copii)
    double content_width;  // lățime - 2 * padding - 2 * border
    double content_height; // înălțime - 2 * padding - 2 * border

    // 4. COORDONATE CONȚINUT (Începutul spațiului de conținut) - În sistemul Layout (Top-Down)
    double x_content_start; // x_start + BorderLeft + PaddingLeft
    double y_content_start; // y_start - BorderTop - PaddingTop // <-- CRUCIAL: Se scade!

    // 5. FLOW END (Poziția Y pentru următorul element)
    double next_element_y;  // y_end - MarginBottom
    double next_element_x;  // x_end - MarginBottom
};

struct Cell {
    BoxMetrics cell_metrics;
    const XhtmlElement* xhtml_element_ref = nullptr;
    size_t internal_element_id;
    int colspan = 1;
    int rowspan = 1;
};

struct Row {
    std::vector<Cell> cells;

    BoxMetrics row_metrics;
    const XhtmlElement* xhtml_element_ref = nullptr;

    size_t internal_element_id;
};

class Table {
public:
    Table() = default;

    const XhtmlElement* xhtml_element_ref = nullptr;
    size_t internal_element_id;

    std::vector<Row> rows;

    std::vector<double> column_widths;
    std::vector<double> column_x_start;

    std::vector<double> row_heights;
    std::vector<double> row_y_start;

    void print();

    int getMaxColumns() {
        int max_columns = 0;
        for (const auto& row : rows) {
            int column_count = 0;
            for (const auto& cell : row.cells) {
                column_count += cell.colspan;
            }
            if (column_count > max_columns) {
                max_columns = column_count;
            }
        }
        return max_columns;
    }
};

class RenderingContext {
private:
    // Referință la pagina curentă (nu deține pagina)
    

public:
    const Page* m_page;
    Style style; // Toate proprietățile de stil sunt aici!
    //XhtmlElement curent_xhtml_element;
    const XhtmlElement* current_xhtml_element;
    BoxMetrics metrics;
    //Table table_data;
    //std::vector<Table> table_stack;

    Table* current_table_ref = nullptr;
    
     
    // ==========================================================
    // 1. Proprietăți de Flow (Cursor și Limite)
    // ==========================================================

    double cursor_x = 0.0;
    double cursor_y = 0.0; // Cursor_y începe de sus (după Page::getMarginTop()) și crește în jos.

  

   // double content_start_y = 0.0;
    // Lățimea disponibilă (dincolo de care textul trebuie să treacă la linie nouă). 
    // Aceasta se calculează din Page::getContentWidth()
    //double flow_width_limit = 0.0;

    // Marginea de start X (de unde începe orice bloc nou). 
    // Egală inițial cu Page::getMarginLeft().
    //double margin_x = 0.0;
    //double current_line_start_x = 0.0; // Inițial, aceeași cu margin_x
    // ==========================================================
    // 2. Stare și Flag-uri
    // ==========================================================

    //bool current_line_has_content = false;
    //bool last_item_was_space = false;
    //bool is_measuring_content = false; // Flag Two-Pass (înlocuiește is_measuring_table_content)
    //bool inside_table_cell = false;
    int depth = 0; // Util pentru debug sau stiluri specifice de adâncime


   // bool shouldRenderBackground = false;
   // bool shouldRenderBorder = false;
   
    // ==========================================================
    // 4. Constructor & Utility
    // ==========================================================


    

    // Constructor care primește referința la pagină
    RenderingContext(const Page* page, const XhtmlElement* element) : m_page(page),
        current_xhtml_element(element) {
        if (m_page) {
            // Seteaza limitele initiale pe baza paginii
            //margin_x = m_page->getMarginLeft();
            cursor_x = m_page->getMarginLeft();
            cursor_y = m_page->getMarginTop();
            //flow_width_limit = m_page->getContentWidth();
            //current_line_start_x = metrics.x_content_start;

          
        }
    }

    RenderingContext(const RenderingContext& other)
        : m_page(other.m_page), // Copie referința la pagină
        style(other.style),
        current_xhtml_element(other.current_xhtml_element),
        metrics(other.metrics),
        current_table_ref(other.current_table_ref),
        // 🎯 COPIEREA MEMBRILOR DE FLOW CRITICI 🎯
        cursor_x(other.cursor_x),
        cursor_y(other.cursor_y), // <-- CRITIC! Acesta trebuie copiat
        //content_start_y(other.content_start_y),
        //flow_width_limit(other.flow_width_limit),
        //margin_x(other.margin_x),
        //current_line_start_x(other.current_line_start_x),

        // COPIEREA FLAG-URILOR DE STARE
        //current_line_has_content(other.current_line_has_content),
        //last_item_was_space(other.last_item_was_space),
        //is_measuring_content(other.is_measuring_content),
        //inside_table_cell(other.inside_table_cell),
        depth(other.depth)
       // table_stack(other.table_stack)
        //shouldRenderBackground(other.shouldRenderBackground),
        //shouldRenderBorder(other.shouldRenderBorder)
    {
        // Corpul constructorului poate rămâne gol
    }

    // Operator de Atribuire Corectat
    RenderingContext& operator=(const RenderingContext& other) {
        if (this != &other) {
            m_page = other.m_page; // Referința la pagină se poate copia
            style = other.style;
            current_xhtml_element = other.current_xhtml_element;
            metrics = other.metrics;

            // 🎯 COPIEREA MEMBRILOR DE FLOW CRITICI 🎯
            cursor_x = other.cursor_x;
            cursor_y = other.cursor_y;
            //content_start_y = other.content_start_y;
            //flow_width_limit = other.flow_width_limit;
            //margin_x = other.margin_x;
            //current_line_start_x = other.current_line_start_x;

            // COPIEREA FLAG-URILOR DE STARE
            //current_line_has_content = other.current_line_has_content;
            //last_item_was_space = other.last_item_was_space;
            //is_measuring_content = other.is_measuring_content;
            //inside_table_cell = other.inside_table_cell;
            depth = other.depth;
            //table_stack = other.table_stack;
           // shouldRenderBackground = other.shouldRenderBackground;
           // shouldRenderBorder = other.shouldRenderBorder;
            current_table_ref = other.current_table_ref;
        }
        return *this;
    }

    // Metodă pentru a obține lățimea paginii (utilă pentru calcule)
    double getPageContentWidth() const {
        return m_page ? m_page->getContentWidth() : metrics.content_width;
    }

    // Metodă pentru a calcula lățimea rămasă pe linia curentă
    double getRemainingWidthOnLine() const {
        // Lățimea totală a flow-ului minus poziția cursorului
        //return flow_width_limit - (cursor_x - margin_x);
        return metrics.content_width - (cursor_x - metrics.x_content_start);
    }

  


    void print(const std::wstring& header) const {

        // Asigură-te că LOG_DEBUG este definit și funcționează cu std::wstring
        // (Presupunând că ai definit macro-uri ca LOG_DEBUG(msg) -> ConsoleManager::getInstance().log(msg, LogLevel::DEBUG))

        LOG_DEBUG(L"=========================================================");
        LOG_DEBUG(L"=== CONTEXT DUMP: " + header + L" ===");
        LOG_DEBUG(L"=========================================================");

        // 1. Flow (Cursor și Limite)
        LOG_DEBUG(L"[FLOW]");
        LOG_DEBUG(L"  Element Tag: <" + current_xhtml_element->getTagName() + L"> (Depth: " + std::to_wstring(depth) + L")");
        LOG_DEBUG(L"  Cursor X/Y: (" + std::to_wstring(cursor_x) + L", " + std::to_wstring(cursor_y) + L")");
        //LOG_DEBUG(L"  Content Start Y: " + std::to_wstring(content_start_y));
        //LOG_DEBUG(L"  Flow/Line Limit: " + std::to_wstring(flow_width_limit) + L" (Remaining: " + std::to_wstring(getRemainingWidthOnLine()) + L")");
        //LOG_DEBUG(L"  Margin X Start: " + std::to_wstring(margin_x) + L" | Line Start X: " + std::to_wstring(current_line_start_x));

        // 2. Stare și Flag-uri
        LOG_DEBUG(L"[STATE]");
        //LOG_DEBUG(L"  Line Has Content: " + (current_line_has_content ? std::wstring(L"Yes") : std::wstring(L"No")));
        //LOG_DEBUG(L"  Render Flags: BG(" + (shouldRenderBackground ? std::wstring(L"Yes") : std::wstring(L"No")) + L"), Border(" + (shouldRenderBorder ? std::wstring(L"Yes") : std::wstring(L"No")) + L")");
        //LOG_DEBUG(L"  Inside Table: " + (inside_table_cell ? std::wstring(L"Yes") : std::wstring(L"No")) + L" | Measuring: " + (is_measuring_content ? std::wstring(L"Yes") : std::wstring(L"No")));

        // 3. Stilul Curent
        LOG_DEBUG(L"[STYLE]");
        style.print();

        LOG_DEBUG(L"=========================================================");
    }
    
};
