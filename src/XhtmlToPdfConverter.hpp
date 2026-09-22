#include "Xhtml.hpp"         
#include "PdfWriterWrapper.hpp" 
//#include "RenderingContex.hpp"

struct RenderingContext_old {
    double page_width = 210.0;
    double page_height = 297.0;
    double cursor_x = 0.0;
    double cursor_y = 0.0;

    std::string current_font_name = "Arial";
    double current_font_size = 12.0;
    bool is_bold = false;
    bool is_italic = false;

    double margin_top = 10.0;
    double margin_x = 10.0;

    std::wstring text_align = L"left";     // Adăugați text-align
    std::wstring vertical_align = L"top";  // Adăugați vertical-align

    bool current_line_has_content = false;
    bool last_item_was_space = false;
    bool is_measuring_table_content = false;
    bool inside_table_cell = false;
    int depth = 0;

    ColorRgb current_text_color = { 0.0, 0.0, 0.0 };
    ColorRgb current_background_color = { -1.0, -1.0, -1.0 }; // Folosim -1.0 ca sentinelă pentru 'transparent/unset'

    // === NOILE PROPRIETĂȚI PENTRU BORDURĂ ===
    std::wstring current_border_style = L"none"; // ex: "none", "solid", "hidden", "dashed"
    double current_border_width = 0.0;          // lățimea bordurii în puncte (pt)
    ColorRgb current_border_color = { 0.0, 0.0, 0.0 }; // Culoarea bordurii (implicit negru)
    // =========================================
};


struct CellMetadata {
    const XhtmlElement* cellElement;
    int colSpan;
    int rowSpan; // Pregătire pentru viitor (setat acum la 1)
};

// Definiți o structură pentru a stoca celulele pe măsură ce le parcurgeți
struct TableCell {
    std::wstring content; // Conținutul celulei (textul)
    // Putem extinde mai târziu pentru alte atribute (colspan, rowspan)
};

struct TableRow {
    std::vector<TableCell> cells;
};


struct CellContentState {
    const XhtmlElement* cellElement; // Referința la elementul <td>/<th>
    RenderingContext_old context;        // Contextul de randare specific celulei (cursor_x, cursor_y, stil)
    double max_content_x;            // Limita de wrap X
};

struct TableRenderData {
    //std::vector<std::vector<const XhtmlElement*>> rows;
    std::vector<std::vector<CellMetadata>> rows;

    std::vector<const XhtmlElement*> rowElements;
    // NOU: Grilă care indică câte rânduri rămân de acoperit(0 = liber / neocupat)
    // Dimensiune: rows.size() x numColumns
    std::vector<std::vector<int>> occupiedGrid;

    int numColumns = 0;
    double tableWidth = 0.0;
    double colWidth = 0.0;
    double cellPadding = 0.0;

    double rowStart_y = 0.0;
    double max_final_y_global = 0.0;

    std::vector<double> rowHeights;
    std::vector<std::pair<CellContentState, double>> cellDrawData;

    std::vector<double> fixedColWidths;
    double fixedRowHeight = 0.0;
    bool isFixedLayout = false;
};

class PdfConverter_old {
private:
    const Xhtml& m_xhtml;

    PdfWriterWrapper m_pdfWriter;
    RenderingContext_old m_context;
    

public:
    PdfConverter_old(const Xhtml& xhtml);
    bool convert(const std::wstring& outputFilePath);

private:
    // Metoda recursivă principală (Dispatcher)
    void processNodeRecursive(const XhtmlElement& element);

    // Functii ajutătoare de randare (primesc contextul prin referință)
    void drawText(const std::wstring& text, RenderingContext_old& currentContext);
    void processContentAndChildren(const XhtmlElement& element, RenderingContext_old& currentContext);

    // Functii de procesare specifice tag-urilor (primesc contextul prin referință)
    void processH1(const XhtmlElement& element, RenderingContext_old& currentContext);
    void processP(const XhtmlElement& element, RenderingContext_old& currentContext);
    
    void processTable(const XhtmlElement& element, RenderingContext_old& currentContext);
    void processImg(const XhtmlElement& element, RenderingContext_old& currentContext);
   

    void processSpan(const XhtmlElement& element, RenderingContext_old& currentContext);
    void processGenericBlock(const XhtmlElement& element, RenderingContext_old& currentContext);
    void processInline(const XhtmlElement& element, RenderingContext_old& currentContext);
    //void processContentAsText(const std::wstring& content, RenderingContext& currentContext, double max_content_x_limit = -1.0);
    void processContentAsText(const XhtmlElement& textElement, RenderingContext_old& currentContext, double max_content_x_limit = -1.0);
    double getCalculatedTableWidth() const;
    double getCalculatedContentWidth(const RenderingContext_old& context) const;


    // Functii utilitare (nu modifică cursorul, pot rămâne ca metode simple)
    void applyCssToContext(const std::wstring& tagName, const std::map<std::wstring, std::wstring>& attributes);
    void drawBlock(const XhtmlElement& element); // Nu este folosită momentan, dar păstrăm semnătura
    bool isBlockElement(const std::wstring& tagName) const;

    std::wstring extractNestedText(const XhtmlElement& element);


    void renderCellContents(const TableRenderData& tableData, RenderingContext_old& currentContext);
    void drawTableStructure(const TableRenderData& tableData, RenderingContext_old& currentContext);

    void calculateRowHeightsAndCellContexts(TableRenderData& tableData, RenderingContext_old& currentContext);
    void calculateFixedRowHeightsAndCellContexts(TableRenderData& tableData, RenderingContext_old& currentContext);

    bool collectTableDataAndSetup(const XhtmlElement& element, const RenderingContext_old& currentContext, TableRenderData& tableData);


    double convertCssLengthToPt(const std::wstring& cssValue) const;
    double getCellPaddingFromCss() const;


    //br
    void processLineBreak(RenderingContext_old& currentContext);

    void printTableGrid(const TableRenderData& tableData);

    ColorRgb parseCssColorToRgb(const std::wstring& css_color_val); //mutat in Style

    std::string decodeBase64(const std::wstring& encoded_wstring);
    
};