
#include "XhtmlToPdfConverter.hpp"
#include "ConsoleManager.hpp" 

#include <tuple> 
#include <algorithm>
#include <cwctype> 

#include <string>
#include <vector>
#include <stdexcept>

// Asigură-te că includeți și celelalte fișiere necesare

PdfConverter_old::PdfConverter_old(const Xhtml& xhtml)
    : m_xhtml(xhtml) {
    // Inițializarea contextului de bază (Unități în Puncte (pt): 1pt = 1/72 inchi)

    m_context.page_width = 210.0 * 72 / 25.4; // A4 Lățime în pt
    m_context.page_height = 297.0 * 72 / 25.4; // A4 Înălțime în pt

    double margin_pt = 10.0 * 72 / 25.4; // 10 mm în pt (~28.35 pt)
    m_context.margin_x = margin_pt; // Marginea stânga/dreapta
    m_context.margin_top = margin_pt; // Marginea sus

    // B. INIȚIALIZARE CORECTĂ A CURSORULUI
    m_context.cursor_x = m_context.margin_x;
    m_context.cursor_y = m_context.page_height - m_context.margin_top;

    m_context.current_font_name = "Arial";
}

bool PdfConverter_old::convert(const std::wstring& outputFilePath) {
    LOG_INFO(L"[PDF] Începe conversia la PDF.");


    try {
        std::locale::global(std::locale(""));
        // sau o setare explicită, dacă cea de mai sus nu funcționează (depinde de sistemul de operare)
        // std::locale::global(std::locale("ro_RO.utf8")); 
    }
    catch (...) {
        LOG_WARNING(L"[LOCALE] Nu s-a putut seta locale-ul global. Pot apărea probleme cu diacriticele.");
    }

    if (!m_pdfWriter.initialize(outputFilePath, m_context.page_width, m_context.page_height)) {
        LOG_ERROR(L"[PDF] Eroare la inițializarea PdfWriter.");
        return false;
    }

    m_pdfWriter.startPage(m_context.page_width, m_context.page_height);
   
    const XhtmlElement& rootElement = m_xhtml.getRoot();

    if (rootElement.tagName == L"html") {
        LOG_DEBUG(L"[PDF Processor] Rădăcină 'html' găsită. Începe procesarea recursivă.");
        processNodeRecursive(rootElement);
    }
    else {
        LOG_ERROR(L"[PDF] Rădăcina documentului (tag <" + rootElement.tagName + L">) nu este <html>. Conversie eșuată.");
        m_pdfWriter.endPage();
        m_pdfWriter.finalize();
        return false;
    }

    m_pdfWriter.endPage();
    m_pdfWriter.finalize();
    LOG_SUCCESS(L"[PDF] Conversie finalizată cu succes la: " + outputFilePath);
    return true;
}

// ---------------------------------------------------------------------------------
// FUNCTII AJUTĂTOARE (Primesc Context&)
// ---------------------------------------------------------------------------------

void PdfConverter_old::drawText(const std::wstring& text, RenderingContext_old& currentContext) {
    if (text.empty()) return;

    std::wstring cleanedText = text;
    
    // Cazul Spațiu Canonic 
    if (cleanedText == L" ") {
        //double spaceWidth = m_pdfWriter.measureText(L" ", currentContext.current_font_size);
        double spaceWidth = m_pdfWriter.measureText(L" ", currentContext.current_font_size);
        
        // CORECȚIE CRITICĂ: Mărește lățimea spațiului dacă măsurarea e incorectă (0.0pt sau prea mică)
        if (spaceWidth < 1.0) {
            spaceWidth = currentContext.current_font_size * 0.5;
            LOG_WARNING(L"[MEASURE] CORECȚIE Spațiu Zero: Lățimea 0.0 forțată la " + std::to_wstring(spaceWidth));
        }

        currentContext.cursor_x += spaceWidth;
        currentContext.current_line_has_content = true;
        currentContext.last_item_was_space = true;
        return;
    }
    
    // B. SETEAZĂ FONTUL
    /*
    m_pdfWriter.setFont(currentContext.current_font_name,
        currentContext.current_font_size,
        currentContext.is_bold,
        currentContext.is_italic);
    */
    // C. MĂSURAREA ȘI CORECTAREA LĂȚIMII
    double textWidth = m_pdfWriter.measureText(cleanedText, currentContext.current_font_size);

    // Corecție Euristică: Plasa de Siguranță
    // Dacă lățimea măsurată este sub 50% din lățimea minimă așteptată, aplicăm corecția
    double minExpectedWidth = cleanedText.length() * currentContext.current_font_size * 0.60;

    if (textWidth < minExpectedWidth * 0.5) {
        // Aplicăm lățimea euristică calculată
        LOG_WARNING(L"[MEASURE] CORECȚIE EURISTICĂ: Lățime " + std::to_wstring(textWidth) +
            L" forțată la " + std::to_wstring(minExpectedWidth) + L" (Lungime * 0.6).");
        textWidth = minExpectedWidth;
    }

//    LOG_DEBUG(L"[DRAW] Text: \"" + cleanedText + L"\", X: " + std::to_wstring(currentContext.cursor_x));

    // =========================================================================
    // D. CORECȚIE CRITICĂ: DESENAREA FUNDALULUI (RECTANGLE FILL)
    // =========================================================================

    // Verificăm dacă culoarea de fundal nu este transparentă 
    // (presupunem r >= 0.0 indică o culoare setată)
   // Verificăm dacă culoarea de fundal nu este transparentă 

    LOG_DEBUG(L"[DIAGNOSTIC] BG Color R-Value for text \"" + cleanedText + L"\": " +
        std::to_wstring(currentContext.current_background_color.r));

    if (currentContext.current_background_color.r >= 0.0) {

        // 1. Calculează Dimensiunile

        // Înălțimea dreptunghiului (Generos: 1.5 ori mărimea fontului pentru a acoperi descendenții/ascendenții + padding)
        double rect_height = currentContext.current_font_size * 1.5;

        // Y_bottom: Colțul stânga-jos. Îl coborâm cu 0.5 * FontSize de la baseline (cursor_y)
        double rect_y_bottom = currentContext.cursor_y - currentContext.current_font_size * 0.5;

        // Adăugăm un mic padding orizontal
        double padding_x = 1.0;
        double final_x = currentContext.cursor_x - (padding_x / 2.0); // Începe puțin în stânga cursorului
        double final_width = textWidth + padding_x;                   // Se termină puțin în dreapta

        // 2. APEL CORECT: Folosim addRectangle cu coordonatele dinamice
        m_pdfWriter.addRectangle(
            final_x,
            rect_y_bottom,
            final_width,
            rect_height,
            currentContext.current_background_color, // << Culoarea YELLOW din context
            0.0,
            { 0.0, 0.0, 0.0 }
        );

        LOG_DEBUG(L"  [BACKGROUND] Desenat fundal la X_start:" + std::to_wstring(final_x) +
            L", Y_Bottom:" + std::to_wstring(rect_y_bottom) + L" pt, Width:" + std::to_wstring(final_width));
    }

    m_pdfWriter.addText(
        currentContext.cursor_x,
        currentContext.cursor_y,
        cleanedText,
        currentContext.current_font_size,
        currentContext.current_text_color
    );

    // E. Avansăm cursorul X
    currentContext.cursor_x += textWidth;
    LOG_DEBUG(L"[DRAW] Dupa Text: \"" + cleanedText + L"\", X: " + std::to_wstring(currentContext.cursor_x));
    currentContext.last_item_was_space = false;
    currentContext.current_line_has_content = true;
}


void PdfConverter_old::processContentAndChildren(const XhtmlElement& element, RenderingContext_old& currentContext) {
    // A. TRATEAZĂ CONȚINUTUL TEXT
    if (!element.content.empty()) {
        //processContentAsText(element.content, currentContext);
        processContentAsText(element, currentContext);
    }

    // B. PROCESEAZĂ SUB-ELEMENTELE (Recursivitate)
    for (const auto& child : element.subElements) {
        // Logica de transfer m_context = currentContext; / currentContext = m_context;
        // NU MAI ESTE NECESARĂ aici.
        // processNodeRecursive(child) folosește m_context și i se actualizează poziția
        // din pasul 6 al funcției processNodeRecursive.
        processNodeRecursive(child);
    }
}
// ---------------------------------------------------------------------------------
// FUNCTII DE PROCESARE CONTINUT (Handleri Specializati - primesc Context&)
// ---------------------------------------------------------------------------------

void PdfConverter_old::processH1(const XhtmlElement& element, RenderingContext_old& currentContext) {
    processContentAndChildren(element, currentContext);
}

void PdfConverter_old::processP(const XhtmlElement& element, RenderingContext_old& currentContext) {
    processContentAndChildren(element, currentContext);
}

void PdfConverter_old::processSpan(const XhtmlElement& element, RenderingContext_old& currentContext) {
   processContentAndChildren(element, currentContext);
}

void PdfConverter_old::processGenericBlock(const XhtmlElement& element, RenderingContext_old& currentContext) {
   processContentAndChildren(element, currentContext);
}

void PdfConverter_old::processInline(const XhtmlElement& element, RenderingContext_old& currentContext) {
    processContentAndChildren(element, currentContext);
}

// ---------------------------------------------------------------------------------
// FUNCTII UTULITARE 
// ---------------------------------------------------------------------------------

bool PdfConverter_old::isBlockElement(const std::wstring& tagName) const {
    if (tagName == L"#text") {
        return false;
    }

    return tagName == L"p" ||
        tagName == L"div" ||
        tagName == L"h1" || tagName == L"h2" || tagName == L"h3" ||
        tagName == L"h4" || tagName == L"h5" || tagName == L"h6" ||
        tagName == L"li" || tagName == L"ul" || tagName == L"ol" ||
        tagName == L"body" || tagName == L"table" || tagName == L"tr" || tagName == L"td";
}


// O implementare ajutătoare necesară pentru a curăța valoarea font-family.
// De preferat, această funcție ar trebui să fie definită în clasa PdfConverter sau ca helper.
// Aici, o definim ca lambda pentru a o introduce direct în context, dar ar trebui să fie o funcție utilitară.
auto cleanFontFamily = [](const std::wstring& css_value) -> std::wstring {
    if (css_value.empty()) {
        return L"";
    }

    // 1. Extrage prima familie de fonturi (înainte de prima virgulă)
    size_t comma_pos = css_value.find(L',');
    std::wstring primary_font_wstr = (comma_pos == std::wstring::npos)
        ? css_value
        : css_value.substr(0, comma_pos);

    // 2. Elimină spațiile albe de la început și sfârșit (TRIM)
    size_t first = primary_font_wstr.find_first_not_of(L" \t\r\n");
    if (std::wstring::npos == first) {
        return L"";
    }
    size_t last = primary_font_wstr.find_last_not_of(L" \t\r\n");
    primary_font_wstr = primary_font_wstr.substr(first, (last - first + 1));

    // 3. Elimină ghilimelele (dacă există)
    if (primary_font_wstr.length() >= 2) {
        if ((primary_font_wstr.front() == L'"' && primary_font_wstr.back() == L'"') ||
            (primary_font_wstr.front() == L'\'' && primary_font_wstr.back() == L'\'')) {
            primary_font_wstr = primary_font_wstr.substr(1, primary_font_wstr.length() - 2);
        }
    }

    return primary_font_wstr;
};


void PdfConverter_old::applyCssToContext(const std::wstring& tagName, const std::map<std::wstring, std::wstring>& attributes) {

    // Definește dimensiunea implicită a fontului, folosită pentru moștenire
    const double DEFAULT_FONT_SIZE_PT = 12.0;

    // --- LOG START ---
    std::wstring classAttr;
    if (attributes.count(L"class")) {
        classAttr = L" (Class: " + attributes.at(L"class") + L")";
    }
    LOG_DEBUG(L"Aplicare CSS pe <" + tagName + classAttr + L">. Context anterior: Font=" +
        str_to_wstr(m_context.current_font_name) +
        L", Size=" + std::to_wstring(m_context.current_font_size));
    // --- LOG END ---

    // ======================================================================
    // LOGICĂ NOUĂ: Funcție ajutătoare pentru a citi proprietăți din TAG sau CLASĂ
    // ======================================================================
    auto getEffectiveCssValue = [&](const std::wstring& prop) -> std::wstring {
        std::wstring effective_val;

        // 1. Caută valoarea în clase (specificitate mai mare, ex: .font-alt)
        if (attributes.count(L"class")) {
            std::wstring classList = attributes.at(L"class");
            for (const auto& className : wexplode(classList, L' ')) {
                if (className.empty()) continue;
                // Caută pe baza selectorului de clasă (.font-alt)
                effective_val = m_xhtml.getStyles().getPropertyValue(L"." + className, prop);
                if (!effective_val.empty()) {
                    return effective_val; // Prioritizăm clasa găsită
                }
            }
        }

        // 2. Caută valoarea pe baza tag-ului (specificitate mai mică, ex: p, span)
        effective_val = m_xhtml.getStyles().getPropertyValue(tagName, prop);
        return effective_val;
    };
    // ======================================================================


    // --- A. LOGICĂ DE FONTURI (FONT FAMILY, SIZE, WEIGHT, STYLE) ---

    // 1. FONT FAMILY
    std::wstring css_font_family_raw = getEffectiveCssValue(L"font-family");
    std::wstring css_font_family_clean = cleanFontFamily(css_font_family_raw);

    if (!css_font_family_clean.empty()) {
        m_context.current_font_name = wstr_to_str(css_font_family_clean);
        LOG_DEBUG(L"  [FONT FAMILY] Aplicat din CSS (Curățat): " + css_font_family_clean + L" (Raw: " + css_font_family_raw + L")");
    }

    // 2. FONT SIZE (cu logica de moștenire)
    std::wstring css_font_size_val = getEffectiveCssValue(L"font-size");

    // === CORECȚIE MOȘTENIRE FONT: Împiedică resetarea tag-urilor inline (b, #text) la 12pt ===
    bool is_inline_tag_with_no_specific_css = (
        (tagName == L"b" || tagName == L"strong" || tagName == L"span" || tagName == L"#text" || tagName == L"i" || tagName == L"em") &&
        css_font_size_val.empty()
        );

    if (is_inline_tag_with_no_specific_css && m_context.current_font_size != DEFAULT_FONT_SIZE_PT) {
        // Păstrează valoarea moștenită (ex: 7.0866pt) de la elementul părinte (TD)
        LOG_DEBUG(L"  [FONT SIZE] Moștenire Păstrată: Elementul inline menține font-size: " + std::to_wstring(m_context.current_font_size) + L" pt.");
    }
    else
        // === SFÂRȘIT CORECȚIE MOȘTENIRE ===

        // Aplică dimensiunea din CSS (dacă valoarea a fost găsită de getEffectiveCssValue)
        if (!css_font_size_val.empty()) {
            double size_pt = convertCssLengthToPt(css_font_size_val);
            if (size_pt > 0.0) {
                m_context.current_font_size = size_pt;
                LOG_DEBUG(L"  [FONT SIZE] Aplicat din CSS: " + css_font_size_val + L" -> " + std::to_wstring(size_pt) + L" pt");
            }
        }

    // Suprascriere explicită pentru <h1> (din logica veche, dacă nu există CSS)
    if (tagName == L"h1" && css_font_size_val.empty()) {
        m_context.current_font_size = 24.0;
        LOG_DEBUG(L"  [FONT SIZE] Suprascris H1 (Default): 24.0 pt");
    }

    // ====================================================================
    // 🚀 CORECȚIE CRITICĂ FONT SIZE ANTET (2.5mm = 7.0866pt)
    // ====================================================================
    const double HEADER_FONT_SIZE_PT = 7.0866;

    // Se aplică DOAR pe TD/TH cu clasele specifice.
    if (tagName == L"td" || tagName == L"th") {
        if (attributes.count(L"class")) {
            std::wstring classList = attributes.at(L"class");
            if (classList.find(L"header-col1") != std::wstring::npos ||
                classList.find(L"header-col2") != std::wstring::npos)
            {
                m_context.current_font_size = HEADER_FONT_SIZE_PT;
                LOG_DEBUG(L"  [FONT SIZE] ⚠️ **SUPRASCRIERE FORȚATĂ:** Fortat la " +
                    std::to_wstring(HEADER_FONT_SIZE_PT) + L" pt (Clasa Header).");
            }
        }
    }
    // ====================================================================

    // 3. FONT WEIGHT (BOLD)
    std::wstring css_font_weight_val = getEffectiveCssValue(L"font-weight");
    bool bold_set_by_css = false;

    if (css_font_weight_val == L"bold" || css_font_weight_val == L"700" || css_font_weight_val == L"bolder") {
        m_context.is_bold = true;
        bold_set_by_css = true;
        LOG_DEBUG(L"  [BOLD] Setat la TRUE (CSS: " + css_font_weight_val + L")");
    }
    else if (css_font_weight_val == L"normal" || css_font_weight_val == L"400" || css_font_weight_val == L"lighter") {
        m_context.is_bold = false;
        bold_set_by_css = true;
        LOG_DEBUG(L"  [BOLD] Setat la FALSE (CSS: " + css_font_weight_val + L")");
    }

    // Suprascriere prin tag-ul semantic/legacy dacă CSS nu a setat explicit
    if (!bold_set_by_css) {
        if (tagName == L"b" || tagName == L"strong" || tagName == L"h1" ||
            (tagName == L"span" && attributes.count(L"class") && attributes.at(L"class") == L"bold")) {
            m_context.is_bold = true;
            LOG_DEBUG(L"  [BOLD] Setat la TRUE (Tag/Clasă semantică)");
        }
    }

    // 4. FONT STYLE (ITALIC)
    std::wstring css_font_style_val = getEffectiveCssValue(L"font-style");
    bool italic_set_by_css = false;

    if (css_font_style_val == L"italic" || css_font_style_val == L"oblique") {
        m_context.is_italic = true;
        italic_set_by_css = true;
        LOG_DEBUG(L"  [ITALIC] Setat la TRUE (CSS: " + css_font_style_val + L")");
    }
    else if (css_font_style_val == L"normal") {
        m_context.is_italic = false;
        italic_set_by_css = true;
        LOG_DEBUG(L"  [ITALIC] Setat la FALSE (CSS: " + css_font_style_val + L")");
    }

    // Suprascriere prin tag-ul semantic/legacy dacă CSS nu a setat explicit
    if (!italic_set_by_css) {
        if (tagName == L"i" || tagName == L"em") {
            m_context.is_italic = true;
            LOG_DEBUG(L"  [ITALIC] Setat la TRUE (Tag semantic)");
        }
    }

    // --- C. LOGICĂ DE CULOARE (COLOR ȘI BACKGROUND-COLOR) ---

    // 1. CULOAREA TEXTULUI (COLOR)
    std::wstring css_text_color = getEffectiveCssValue(L"color");
    if (!css_text_color.empty()) {
        m_context.current_text_color = parseCssColorToRgb(css_text_color);
        LOG_DEBUG(L"  [COLOR] Aplicat din CSS: " + css_text_color);
    }

    // 2. CULOAREA DE FUNDAL (BACKGROUND-COLOR)
    std::wstring css_background_color = getEffectiveCssValue(L"background-color");
    if (!css_background_color.empty()) {
        m_context.current_background_color = parseCssColorToRgb(css_background_color);
        LOG_DEBUG(L"  [BACKGROUND-COLOR] Aplicat din CSS: " + css_background_color);
    }

    // ----------------------------------------------------------------------
    // --- D. LOGICĂ BORDURI (NOU) ---
    // NOTĂ: Proprietățile de bordură trebuie adăugate la structura RenderingContext
    // ----------------------------------------------------------------------

    // 1. BORDER-STYLE
    std::wstring css_border_style = getEffectiveCssValue(L"border-style");
    if (!css_border_style.empty()) {
        m_context.current_border_style = css_border_style;
        LOG_DEBUG(L"  [BORDER-STYLE] Aplicat din CSS: " + css_border_style);
    }

    // 2. BORDER-WIDTH
    std::wstring css_border_width = getEffectiveCssValue(L"border-width");
    if (!css_border_width.empty()) {
        double width_pt = convertCssLengthToPt(css_border_width);
        if (width_pt >= 0.0) {
            m_context.current_border_width = width_pt;
            LOG_DEBUG(L"  [BORDER-WIDTH] Aplicat din CSS: " + css_border_width + L" -> " + std::to_wstring(width_pt) + L" pt");
        }
    }

    // 3. BORDER-COLOR
    std::wstring css_border_color = getEffectiveCssValue(L"border-color");
    if (!css_border_color.empty()) {
        m_context.current_border_color = parseCssColorToRgb(css_border_color);
        LOG_DEBUG(L"  [BORDER-COLOR] Aplicat din CSS: " + css_border_color);
    }

    // 4. BORDER (Shorthand) - Logica pentru 'border: none'
    std::wstring css_border = getEffectiveCssValue(L"border");
    if (css_border == L"none") {
        // Dacă s-a găsit 'border: none', suprascrie explicit proprietățile de bordură.
        m_context.current_border_style = L"none";
        m_context.current_border_width = 0.0;
        LOG_DEBUG(L"  [BORDER-SHORTHAND] Aplicat 'none', resetat stil/latime.");
    }

    // --- E. LOGICĂ TEXT/VERTICAL-ALIGN ---

    if (tagName == L"p" || tagName == L"div" || tagName == L"td" || tagName == L"th") {

        // Corecție pentru font size 0
        if (m_context.current_font_size < 1.0) {
            m_context.current_font_size = DEFAULT_FONT_SIZE_PT;
        }

        // 1. ALINIERE ORIZONTALĂ (TEXT-ALIGN)
        std::wstring css_text_align = m_xhtml.getStyles().getPropertyValue(tagName, L"text-align");
        std::wstring align_source = L"CSS";

        // Verificare manuală pentru clasele comune de aliniere (prioritate pe clasă)
        if (attributes.count(L"class")) {
            std::wstring classList = attributes.at(L"class");
            if (classList.find(L"align-center") != std::wstring::npos || classList.find(L"text-center") != std::wstring::npos) {
                css_text_align = L"center";
                align_source = L"Class";
            }
            else if (classList.find(L"align-right") != std::wstring::npos || classList.find(L"text-right") != std::wstring::npos) {
                css_text_align = L"right";
                align_source = L"Class";
            }
        }

        if (!css_text_align.empty()) {
            m_context.text_align = css_text_align;
        }
        else if (attributes.count(L"align")) {
            m_context.text_align = attributes.at(L"align");
            align_source = L"Attr";
        }

        if (!m_context.text_align.empty()) {
            LOG_DEBUG(L"  [H-ALIGN] Setat la " + m_context.text_align + L" (" + align_source + L")");
        }


        // 2. ALINIERE VERTICALĂ (VERTICAL-ALIGN)
        std::wstring css_vertical_align = m_xhtml.getStyles().getPropertyValue(tagName, L"vertical-align");
        std::wstring valign_source = L"CSS";

        // Verificare manuală pentru clasele comune de aliniere verticală (prioritate pe clasă)
        if (attributes.count(L"class")) {
            std::wstring classList = attributes.at(L"class");
            if (classList.find(L"valign-middle") != std::wstring::npos || classList.find(L"v-middle") != std::wstring::npos) {
                css_vertical_align = L"middle";
                valign_source = L"Class";
            }
            else if (classList.find(L"valign-bottom") != std::wstring::npos || classList.find(L"v-bottom") != std::wstring::npos) {
                css_vertical_align = L"bottom";
                valign_source = L"Class";
            }
        }

        if (!css_vertical_align.empty()) {
            m_context.vertical_align = css_vertical_align;
            if (m_context.vertical_align != L"top" && m_context.vertical_align != L"middle" && m_context.vertical_align != L"bottom") {
                m_context.vertical_align = L"top";
                LOG_DEBUG(L"  [V-ALIGN] Valoare invalidă, resetat la top.");
            }
        }
        else if (attributes.count(L"valign")) {
            m_context.vertical_align = attributes.at(L"valign");
            valign_source = L"Attr";
        }

        if (!m_context.vertical_align.empty()) {
            LOG_DEBUG(L"  [V-ALIGN] Setat la " + m_context.vertical_align + L" (" + valign_source + L")");
        }
    }
}

void PdfConverter_old::drawBlock(const XhtmlElement& element) {
    // Funcție placeholder
}

// NOTĂ: Presupunem că semnătura a fost actualizată pentru a primi XhtmlElement
void PdfConverter_old::processContentAsText(const XhtmlElement& textElement, RenderingContext_old& currentContext, double max_content_x_limit) {

    // Extragem conținutul normalizat din element
    const std::wstring& content = textElement.content;

    if (content.empty()) {
        LOG_DEBUG(L"[TEXT] Conținut gol. Ieșire.");
        return;
    }

    // --- Inițializări Context ---
    /*
    m_pdfWriter.setFont(
        currentContext.current_font_name,
        currentContext.current_font_size,
        currentContext.is_bold,
        currentContext.is_italic
    );
    */
    double content_start_x = currentContext.margin_x;
    double max_content_x = (max_content_x_limit > 0.0) ? max_content_x_limit : (currentContext.page_width - currentContext.margin_x);
    double line_available_width = max_content_x - content_start_x;
    double line_height = currentContext.current_font_size * 1.2;
    double space_width = m_pdfWriter.measureText(L" ", currentContext.current_font_size);

    double current_y = currentContext.cursor_y;
    double buffer_start_x = currentContext.cursor_x;

    // =========================================================================
    // HELPER: Calculează poziția X bazată pe aliniere
    // =========================================================================
    auto calculateAlignedDrawX = [&](double line_width) -> double {
        double H_offset = 0.0;
        if (currentContext.text_align == L"center") {
            H_offset = std::max<double>(0.0, (line_available_width - line_width) / 2.0);
        }
        else if (currentContext.text_align == L"right") {
            H_offset = std::max<double>(0.0, line_available_width - line_width);
        }
        return content_start_x + H_offset;
    };


    // =========================================================================
    // GESTIONAREA SPAȚIULUI DE ÎNCEPUT (LEADING SPACE)
    // =========================================================================
    bool needsLeadingSpace = textElement.hasAttribute(L"__xhtml_leading_space") &&
        textElement.getAttribute(L"__xhtml_leading_space") == L"true";

    if (needsLeadingSpace &&
        std::abs(buffer_start_x - content_start_x) > 0.01)
    {
        buffer_start_x += space_width;
    }


    std::wstringstream ss(content);
    std::wstring word;
    std::wstring line_buffer;
    int word_count = 0;

    // --- 2. Procesarea Cuvintelor (Logică de Wrap) ---
    while (ss >> word) {
        word_count++;

        std::wstring word_with_prefix = line_buffer.empty() ? L"" : L" ";
        std::wstring potential_line = line_buffer + word_with_prefix + word;
        double potential_width = m_pdfWriter.measureText(potential_line, currentContext.current_font_size);
        double remaining_space = max_content_x - buffer_start_x;

        if (potential_width > remaining_space) {
            // WRAP LOGIC
            if (!line_buffer.empty()) {
                std::wstring trimmed_line = line_buffer;
                size_t last_char = trimmed_line.find_last_not_of(L' ');
                if (last_char != std::wstring::npos) trimmed_line = trimmed_line.substr(0, last_char + 1);

                double line_width = m_pdfWriter.measureText(trimmed_line, currentContext.current_font_size);
                double final_draw_x = (std::abs(buffer_start_x - content_start_x) < 0.01) ? calculateAlignedDrawX(line_width) : buffer_start_x;

                // ----------------------------------------------------
                // >> CORECȚIE FUNDAL: WRAPPED LINE (LINIE COMPLETĂ) <<
                // ----------------------------------------------------
                if (currentContext.current_background_color.r >= 0.0) {
                    double rect_height = currentContext.current_font_size * 1.2;
                    // Ajustat la 0.75 pentru centrare verticală mai bună pe linia de bază
                    double rect_y_bottom = current_y - currentContext.current_font_size * 0.3;

                    double padding_x = 1.0;
                    double final_x = final_draw_x - (padding_x / 2.0);
                    double final_width = line_width + padding_x;

                    m_pdfWriter.addRectangle(
                        final_x,
                        rect_y_bottom,
                        final_width,
                        rect_height,
                        currentContext.current_background_color,
                        0.0,
                        { 0.0, 0.0, 0.0 }
                    );
                    LOG_DEBUG(L"  [BACKGROUND WRAP] Desenat fundal la X_start:" + std::to_wstring(final_x) +
                        L", Y_Bottom:" + std::to_wstring(rect_y_bottom) + L" pt, Width:" + std::to_wstring(final_width));
                }
                // ----------------------------------------------------

                m_pdfWriter.addText(final_draw_x, current_y, trimmed_line, currentContext.current_font_size, currentContext.current_text_color);

                current_y -= line_height;
                buffer_start_x = content_start_x;
                line_buffer = word;
            }
            else {
                // OVERFLOW LOGIC
                double word_width = m_pdfWriter.measureText(word, currentContext.current_font_size);

                // ----------------------------------------------------
                // >> CORECȚIE FUNDAL: OVERFLOW LINE (CUVÂNT TROP LUNG) <<
                // ----------------------------------------------------
                if (currentContext.current_background_color.r >= 0.0) {
                    double rect_height = currentContext.current_font_size * 1.2;
                    // Ajustat la 0.75 pentru centrare verticală mai bună pe linia de bază
                    double rect_y_bottom = current_y - currentContext.current_font_size * 0.3;

                    double padding_x = 1.0;
                    double final_x = buffer_start_x - (padding_x / 2.0);
                    double final_width = word_width + padding_x;

                    m_pdfWriter.addRectangle(
                        final_x,
                        rect_y_bottom,
                        final_width,
                        rect_height,
                        currentContext.current_background_color,
                        0.0,
                        { 0.0, 0.0, 0.0 }
                    );
                    LOG_DEBUG(L"  [BACKGROUND OVERFLOW] Desenat fundal la X_start:" + std::to_wstring(final_x) +
                        L", Y_Bottom:" + std::to_wstring(rect_y_bottom) + L" pt, Width:" + std::to_wstring(final_width));
                }
                // ----------------------------------------------------

                m_pdfWriter.addText(buffer_start_x, current_y, word, currentContext.current_font_size, currentContext.current_text_color);
                current_y -= line_height;
                buffer_start_x = content_start_x;
                line_buffer.clear();
            }
        }
        else {
            line_buffer = potential_line;
        }
    }

    // --- 3. Desenăm Segmentul Final ---
    if (!line_buffer.empty()) {

        std::wstring trimmed_line = line_buffer;
        size_t last_char = trimmed_line.find_last_not_of(L' ');
        if (last_char != std::wstring::npos) trimmed_line = trimmed_line.substr(0, last_char + 1);

        double line_width = m_pdfWriter.measureText(trimmed_line, currentContext.current_font_size);
        double draw_x;

        if (std::abs(buffer_start_x - content_start_x) < 0.01) {
            draw_x = calculateAlignedDrawX(line_width);
        }
        else {
            // Logica de ajustare a aliniamentului pentru restul spațiului
            draw_x = buffer_start_x;

            if (currentContext.text_align == L"center") {
                double remaining_space = max_content_x - buffer_start_x;
                draw_x += std::max<double>(0.0, (remaining_space - line_width) / 2.0);
            }
            else if (currentContext.text_align == L"right") {
                double remaining_space = max_content_x - buffer_start_x;
                draw_x += std::max<double>(0.0, remaining_space - line_width);
            }
        }

        // ----------------------------------------------------
        // >> CORECȚIE FUNDAL: FINAL SEGMENT (SEGMENT ULTIM) <<
        // ----------------------------------------------------
        if (currentContext.current_background_color.r >= 0.0) {
            double rect_height = currentContext.current_font_size * 1.2;
            // Ajustat la 0.75 pentru centrare verticală mai bună pe linia de bază
            double rect_y_bottom = current_y - currentContext.current_font_size * 0.3;

            double padding_x = 1.0;
            double final_x = draw_x - (padding_x / 2.0);
            double final_width = line_width + padding_x;

            m_pdfWriter.addRectangle(
                final_x,
                rect_y_bottom,
                final_width,
                rect_height,
                currentContext.current_background_color,
                0.0,
                { 0.0, 0.0, 0.0 }
            );
            LOG_DEBUG(L"  [BACKGROUND FINAL] Desenat fundal la X_start:" + std::to_wstring(final_x) +
                L", Y_Bottom:" + std::to_wstring(rect_y_bottom) + L" pt, Width:" + std::to_wstring(final_width));
        }
        // ----------------------------------------------------

        m_pdfWriter.addText(draw_x, current_y, trimmed_line, currentContext.current_font_size, currentContext.current_text_color);

        // --- 4. Logica CORECTĂ de Spațiere pe Baza Atributului ---
        double space_to_add = 0.0;

        if (textElement.hasAttribute(L"__xhtml_trailing_space") &&
            textElement.getAttribute(L"__xhtml_trailing_space") == L"true")
        {
            space_to_add = space_width;
        }
        else {
            space_to_add = 0.0;
        }

        currentContext.cursor_x = draw_x + line_width + space_to_add;
        currentContext.cursor_y = current_y;
        currentContext.current_line_has_content = true;
    }
    else {
        currentContext.cursor_x = content_start_x;
        currentContext.cursor_y = current_y;
        currentContext.current_line_has_content = false;
    }
}

double PdfConverter_old::getCalculatedTableWidth() const {
    return m_context.page_width - 2 * m_context.margin_x;
}


std::wstring PdfConverter_old::extractNestedText(const XhtmlElement& element) {
    std::wstring text = L"";
    if (element.tagName == L"#text") {
        text += element.content;
    }
    // Parcurge recursiv sub-elementele pentru a găsi tot textul imbricat
    for (const auto& child : element.subElements) {
        text += extractNestedText(child);
    }
    return text;
}






double PdfConverter_old::getCalculatedContentWidth(const RenderingContext_old& context) const {
    // Returnează lățimea totală disponibilă pentru conținutul paginii/celulei
    // (Lățime totală - Marginea stângă - Marginea dreaptă)
    return context.page_width - 2 * context.margin_x;
}
// Notă: Presupunem că getCalculatedTableWidth() este definită și returnează lățimea corectă

// Notă: Presupunem că getCalculatedTableWidth() este definită și returnează lățimea corectă



// --- Funcția Principală Refactorizată ---
void PdfConverter_old::processTable(const XhtmlElement& element, RenderingContext_old& currentContext) {
    LOG_DEBUG(L"[TABLE] START - Refactorizat.");

    TableRenderData tableData;

    // 1. COLECTARE DATE ȘI SETUP INIȚIAL
    if (!collectTableDataAndSetup(element, currentContext, tableData)) {
        LOG_WARNING(L"[TABLE] Skipped drawing: No rows or columns found.");
        return;
    }

    // 2. CALCUL ÎNĂLȚIME ȘI STOCARE CONTEXTE (FAZA 1)
    // NOU: Logica de decizie în funcție de layout-ul tabelului
    if (tableData.isFixedLayout) {
        LOG_DEBUG(L"[TABLE] Using FIXED layout calculation.");
        calculateFixedRowHeightsAndCellContexts(tableData, currentContext);
    }
    else {
        LOG_DEBUG(L"[TABLE] Using AUTO layout calculation.");
        calculateRowHeightsAndCellContexts(tableData, currentContext);
    }
    

    // 3. DESENARE CHENARE ȘI FUNDAL (FAZA 2 din original)
    drawTableStructure(tableData, currentContext);

    // 4. REDESENARE CONȚINUT (FAZA 3 din original)
    renderCellContents(tableData, currentContext);

    // 5. LOGICA BLOCK END (Actualizăm contextul global)
    currentContext.cursor_y = tableData.max_final_y_global;
    currentContext.cursor_x = currentContext.margin_x;
    currentContext.current_line_has_content = false;
    currentContext.last_item_was_space = false;

    LOG_DEBUG(L"[TABLE] FINAL - Cursor Y global: " + std::to_wstring(currentContext.cursor_y) + L" pt.");
}

bool PdfConverter_old::collectTableDataAndSetup(
    const XhtmlElement& tableElement,
    const RenderingContext_old& currentContext,
    TableRenderData& tableData)
{
    // Resetare
    tableData.rows.clear();
    tableData.rowElements.clear(); // RESETARE NOUĂ
    tableData.numColumns = 0;

    // --- Funcție lambda pentru a procesa un singur element <tr> ---
    auto process_row_element = [&](const XhtmlElement& rowElement) {
        if (rowElement.tagName != L"tr") {
            return;
        }

        std::vector<CellMetadata> currentRow;
        int currentLogicalCols = 0;

        for (const auto& cellElement : rowElement.subElements) {

            if (cellElement.tagName == L"td" || cellElement.tagName == L"th") {
                int col_span = 1;
                int row_span = 1;

                // CITIRE COLSPAN / ROWSPAN
                if (cellElement.attributes.count(L"colspan")) {
                    try { col_span = std::stoi(cellElement.attributes.at(L"colspan")); if (col_span < 1) col_span = 1; }
                    catch (...) { col_span = 1; }
                }
                if (cellElement.attributes.count(L"rowspan")) {
                    try { row_span = std::stoi(cellElement.attributes.at(L"rowspan")); if (row_span < 1) row_span = 1; }
                    catch (...) { row_span = 1; }
                }

                currentRow.push_back({ &cellElement, col_span, row_span });
                currentLogicalCols += col_span;
            }
        }

        // CORECȚIE CRITICĂ: Adăugăm rândul, chiar dacă nu are celule (pt. coerență).
        // În acest caz, currentRow va fi gol.
        tableData.rows.push_back(currentRow);

        // POPULARE NOUĂ: Adăugăm pointerul la elementul <tr>.
        tableData.rowElements.push_back(&rowElement);

        // Actualizăm numărul maxim de coloane.
        if (currentLogicalCols > tableData.numColumns) {
            tableData.numColumns = currentLogicalCols;
        }
    };

    // 1. COLECTARE DATE (Procesare <thead>, <tbody>, <tfoot>)
    for (const auto& tableChild : tableElement.subElements) {
        if (tableChild.tagName == L"thead" || tableChild.tagName == L"tbody" || tableChild.tagName == L"tfoot") {
            // Caz standard: <tr> sunt copii ai thead/tbody
            for (const auto& rowElement : tableChild.subElements) {
                process_row_element(rowElement);
            }
        }
        else if (tableChild.tagName == L"tr") {
            // Caz non-standard: <tr> este copil direct al <table>
            process_row_element(tableChild);
        }
    }

    if (tableData.rows.empty() || tableData.numColumns == 0) {
        return false;
    }

    // 2. INIȚIALIZARE GRILĂ VIRTUALĂ (occupiedGrid) - Logica neschimbată
    tableData.occupiedGrid.resize(tableData.rows.size());
    for (size_t r = 0; r < tableData.rows.size(); ++r) {
        tableData.occupiedGrid[r].assign(tableData.numColumns, 0);
    }
    // ... (Logica de marcare a rowspan-ului, neschimbată)
    int row_index = 0;
    for (const auto& row : tableData.rows) {
        int logical_col_start_index = 0;
        for (const auto& cellMeta : row) {
            while (logical_col_start_index < tableData.numColumns &&
                tableData.occupiedGrid[row_index][logical_col_start_index] > 0)
            {
                logical_col_start_index++;
            }
            if (logical_col_start_index >= tableData.numColumns) break;

            int colSpan = cellMeta.colSpan;
            int rowSpan = cellMeta.rowSpan;

            if (rowSpan > 1) {
                for (int r_span = 1; r_span < rowSpan; ++r_span) {
                    size_t next_row_index = row_index + r_span;
                    if (next_row_index < tableData.rows.size()) {
                        for (int c_span = 0; c_span < colSpan; ++c_span) {
                            int logical_index_to_mark = logical_col_start_index + c_span;
                            if (logical_index_to_mark < tableData.numColumns) {
                                tableData.occupiedGrid[next_row_index][logical_index_to_mark] = rowSpan - r_span;
                            }
                        }
                    }
                }
            }
            logical_col_start_index += colSpan;
        }
        row_index++;
    }

    // 3. SETUP FINAL ȘI CALCUL LĂȚIME/ÎNĂLȚIME FIXĂ (Integrare CSS) - Logica neschimbată
    // Obține acces la regulile CSS
    const CssDefinition css = m_xhtml.getStyles();

    tableData.tableWidth = getCalculatedTableWidth();
    tableData.cellPadding = getCellPaddingFromCss();
    tableData.rowStart_y = currentContext.cursor_y;
    tableData.max_final_y_global = tableData.rowStart_y;

    // A. DETECTARE TABLE-LAYOUT: FIXED
    std::wstring tableLayout = css.getPropertyValue(L"table", L"table-layout");
    tableData.isFixedLayout = (tableLayout == L"fixed");

    // B. EXTRAGERE VALORI FIXE, dacă layout-ul este fix
    if (tableData.isFixedLayout) {
        LOG_DEBUG(L"[TABLE] Fixed layout detected. Parsing fixed dimensions...");

        // 1. Lățimi coloane fixe (td.col1, td.col2, etc.)
        tableData.fixedColWidths.clear();
        double totalFixedWidth = 0.0;

        for (int c = 1; c <= tableData.numColumns; ++c) {
            std::wstring selector = L"td.col" + std::to_wstring(c);
            std::wstring widthValue = css.getPropertyValue(selector, L"width");

            // Caută pe selectorul generic 'td' dacă selectorul specific lipsește
            if (widthValue.empty()) {
                widthValue = css.getPropertyValue(L"td", L"width");
            }
            // Caută și pe selectorul generic 'th'
            if (widthValue.empty()) {
                widthValue = css.getPropertyValue(L"th", L"width");
            }

            // Utilizarea funcției de conversie furnizate
            double width = convertCssLengthToPt(widthValue);

            if (width == 0.0) {
                LOG_WARNING(L"[TABLE] Width not found or invalid for column " + std::to_wstring(c) + L". Falling back to AUTO.");
                // Dacă lipsește o lățime, dezactivăm Fixed Layout
                tableData.isFixedLayout = false;
                tableData.fixedColWidths.clear();
                break;
            }
            else {
                // LOG_WARNING(L"[TABLE] Width for column " + std::to_wstring(c) + L": " + std::to_wstring(width));
            }

            tableData.fixedColWidths.push_back(width);
            totalFixedWidth += width;
        }

        if (tableData.isFixedLayout) {
            // 2. Înălțime rând fixă (valoarea veche, care va fi suprascrisă dinamic în calculateFixedRowHeightsAndCellContexts)
            // Lăsăm această logică pentru a menține o valoare de referință implicită/minimă.
            std::wstring heightValue = css.getPropertyValue(L"tr.row-default", L"height"); // Am schimbat selectorul la cel implicit.
            if (heightValue.empty()) {
                heightValue = css.getPropertyValue(L"tr", L"height");
            }

            tableData.fixedRowHeight = convertCssLengthToPt(heightValue);

            if (tableData.fixedRowHeight == 0.0) {
                tableData.fixedRowHeight = convertCssLengthToPt(L"10mm");
            }

            // Setăm lățimea tabelei la suma lățimilor fixe
            tableData.tableWidth = totalFixedWidth;
        }
    }

    // C. FINALIZARE PENTRU AUTO LAYOUT
    if (!tableData.isFixedLayout) {
        tableData.colWidth = tableData.tableWidth / tableData.numColumns;
    }

    // #ifdef _DEBUG 
    printTableGrid(tableData);
    // #endif

    return true;
}

void PdfConverter_old::calculateRowHeightsAndCellContexts(
    TableRenderData& tableData,
    RenderingContext_old& currentContext)
{
    // Inițializări
    double rowStart_y = tableData.rowStart_y;
    double cellPadding = tableData.cellPadding;
    double colLogicalWidth = tableData.colWidth;
    double text_block_height_factor = currentContext.current_font_size;

    // CORECȚIE ERORI: Mutăm declarația în scope-ul funcției
    double text_height_above_baseline = currentContext.current_font_size * 0.8;

    // Iterație pe Rânduri
    for (size_t r = 0; r < tableData.rows.size(); ++r) {
        double initial_row_y = rowStart_y;
        double max_final_y_row = initial_row_y;
        bool row_has_content = false;

        size_t cell_index_in_row = 0;

        // ITERARE PE COLOANELE LOGICE (c_logical)
        for (int c_logical = 0; c_logical < tableData.numColumns; ) {

            // 1. VERIFICARE ROWSPAN
            if (r < tableData.occupiedGrid.size() && c_logical < tableData.occupiedGrid[r].size() &&
                tableData.occupiedGrid[r][c_logical] > 0)
            {
                c_logical++;
                continue;
            }

            // 2. VERIFICARE: Mai sunt celule reale?
            if (cell_index_in_row >= tableData.rows[r].size()) {
                c_logical++;
                continue;
            }

            // 3. PROCESARE CELULĂ REALĂ
            const CellMetadata& cellMeta = tableData.rows[r][cell_index_in_row];
            const XhtmlElement* cellElement = cellMeta.cellElement;
            int colSpan = cellMeta.colSpan;

            double current_cell_start_x = currentContext.margin_x + (c_logical * colLogicalWidth);
            double currentCellWidth = colLogicalWidth * colSpan;

            // A. PREGĂTIRE CONTEXT CELULĂ
            RenderingContext_old cellContext = currentContext;
            applyCssToContext(cellElement->tagName, cellElement->attributes);

            cellContext.margin_x = current_cell_start_x + cellPadding;
            double max_content_x = current_cell_start_x + currentCellWidth - cellPadding;

            // Punctul de bază Y de unde începe randarea textului
            // Folosim variabila din scope-ul exterior (NU MAI DECLARĂM AICI)
            double text_baseline_y = initial_row_y - cellPadding - text_height_above_baseline;

            cellContext.cursor_y = text_baseline_y;
            cellContext.cursor_x = cellContext.margin_x;

            // B. Stocare stare inițială
            CellContentState state = { cellElement, cellContext, max_content_x };

            // C. MĂSURARE CONȚINUT (Simulare randare)
            RenderingContext_old originalContext = m_context;
            m_context = cellContext;
            m_context.inside_table_cell = true;
            m_context.current_line_has_content = false;

            for (const auto& child : cellElement->subElements) {
                if (child.tagName == L"#text") {
                    //processContentAsText(child.content, m_context, max_content_x);
                    processContentAsText(child, m_context, max_content_x);
                }
                else if (child.tagName == L"table") {
                    double desired_table_width = currentCellWidth - (2 * cellPadding);
                    double new_page_width = desired_table_width + 2 * currentContext.margin_x;
                    double temp_original_page_width = m_context.page_width;
                    m_context.page_width = new_page_width;
                    m_context.is_measuring_table_content = true;
                    processNodeRecursive(child);
                    m_context.is_measuring_table_content = false;
                    m_context.page_width = temp_original_page_width;
                }
                else {
                    m_context.is_measuring_table_content = true;
                    processNodeRecursive(child);
                    m_context.is_measuring_table_content = false;
                }
            }

            // D. ACTUALIZARE ÎNĂLȚIME RÂND
            double cell_final_y = m_context.cursor_y;

            if (cell_final_y < max_final_y_row || !row_has_content) {
                max_final_y_row = cell_final_y;
            }

            row_has_content = true;

            // Stocare date de desenare
            tableData.cellDrawData.push_back({ state, cell_final_y });

            // RESTAURARE context global
            m_context = originalContext;

            // E. AVANSARE CURSORI
            c_logical += colSpan;
            cell_index_in_row++;
        }

        // F. CALCUL FINAL ÎNĂLȚIME RÂND

        if (!row_has_content) {
            max_final_y_row = initial_row_y - text_block_height_factor - (cellPadding * 2.0);
        }

        // 'text_height_above_baseline' este acum vizibilă aici
        double row_content_start_y_baseline = initial_row_y - cellPadding - text_height_above_baseline;
        double height_from_start_to_end_baseline = row_content_start_y_baseline - max_final_y_row;
        double actual_content_height = height_from_start_to_end_baseline;

        if (height_from_start_to_end_baseline > 0.0) {
            actual_content_height += text_block_height_factor;
        }
        if (actual_content_height < text_block_height_factor) {
            actual_content_height = text_block_height_factor;
        }

        double actual_row_height = actual_content_height + (cellPadding * 2.0);

        // Actualizare rowStart_y și stocare înălțime
        rowStart_y = initial_row_y - actual_row_height;
        tableData.rowHeights.push_back(actual_row_height);
    }

    tableData.max_final_y_global = rowStart_y;
}


void PdfConverter_old::drawTableStructure(
    const TableRenderData& tableData,
    RenderingContext_old& currentContext)
{
    // Setup culori/borduri
    ColorRgb borderColor = { 0, 0, 0 };
    ColorRgb whiteFill = { 1.0, 1.0, 1.0 };
    ColorRgb transparentFill = { -1, -1, -1 };
    double borderWidth = 1.;

    double colLogicalWidth = tableData.colWidth;

    // Y_top_of_row_r: Coordonata Y de sus a rândului curent, pornește de la începutul tabelei
    double cell_y_top_accumulator = tableData.rowStart_y; // Folosim rowStart_y pentru consistență

    // Iterație pe Rânduri
    for (size_t r = 0; r < tableData.rows.size(); ++r) {
        double current_row_height = tableData.rowHeights[r];
        // Y_bottom_of_row_r: Coordonata Y de jos a rândului curent
        double cell_y_bottom_for_row_r = cell_y_top_accumulator - current_row_height;

        size_t cell_index_in_row = 0;

        // Iterație pe COLOANELE LOGICE
        for (int c_logical = 0; c_logical < tableData.numColumns; ) {

            // 1. VERIFICARE ROWSPAN: Celula este ocupată?
            if (r < tableData.occupiedGrid.size() && c_logical < tableData.occupiedGrid[r].size() &&
                tableData.occupiedGrid[r][c_logical] > 0)
            {
                c_logical++;
                continue;
            }

            // 2. Verificare: Mai avem celule reale?
            if (cell_index_in_row >= tableData.rows[r].size()) {
                c_logical++;
                continue;
            }

            // 3. Preluare Celulă Reală
            const CellMetadata& cellMeta = tableData.rows[r][cell_index_in_row];
            int colSpan = cellMeta.colSpan;
            int rowSpan = cellMeta.rowSpan;

            // 4. CORECȚIE CRITICĂ: CALCULUL GEOMETRIEI X (Lățime și Poziție de Start)
            double currentCellWidth = 0.0;
            double current_cell_start_x = currentContext.margin_x;

            if (tableData.isFixedLayout) {
                // FIXED LAYOUT: Calculăm prin însumarea lățimilor fixe (tableData.fixedColWidths)

                // 4.1. Poziția X de start: Suma lățimilor coloanelor precedente
                for (int i = 0; i < c_logical; ++i) {
                    if (i < tableData.fixedColWidths.size()) {
                        current_cell_start_x += tableData.fixedColWidths[i];
                    }
                }

                // 4.2. Lățimea celulei: Suma lățimilor coloanelor acoperite de colSpan
                for (int i = 0; i < colSpan; ++i) {
                    if (c_logical + i < tableData.fixedColWidths.size()) {
                        currentCellWidth += tableData.fixedColWidths[c_logical + i];
                    }
                }

            }
            else {
                // AUTO LAYOUT: Logica originală bazată pe lățimea medie (colLogicalWidth)
                current_cell_start_x = currentContext.margin_x + (c_logical * colLogicalWidth);
                currentCellWidth = colLogicalWidth * colSpan;
            }


            // 5. CALCUL ÎNĂLȚIME ȘI COORDONATĂ Y CORECTĂ (Logică Rowspan)
            double actual_cell_height = current_row_height;
            double cell_y_bottom;

            if (rowSpan > 1) {
                // Dacă e rowspan, sumăm înălțimile rândurilor
                actual_cell_height = 0.0;
                for (int rs = 0; rs < rowSpan; ++rs) {
                    if (r + rs < tableData.rowHeights.size()) {
                        actual_cell_height += tableData.rowHeights[r + rs];
                    }
                }
                cell_y_bottom = cell_y_top_accumulator - actual_cell_height;

            }
            else {
                // Celulă normală: Y de jos este Y_bottom_of_row_r
                cell_y_bottom = cell_y_bottom_for_row_r;
            }


            // 6. DESENARE RECTANGLE (Fill și Stroke)
            // Desenăm fundalul (Fill)
            m_pdfWriter.addRectangle(
                current_cell_start_x, cell_y_bottom, currentCellWidth, actual_cell_height,
                whiteFill, 0.0, borderColor
            );

            // Desenăm bordura (Stroke)
            m_pdfWriter.addRectangle(
                current_cell_start_x, cell_y_bottom, currentCellWidth, actual_cell_height,
                transparentFill, borderWidth, borderColor
            );

            // 7. Avansare Cursori
            c_logical += colSpan;
            cell_index_in_row++;
        }

        // Actualizăm poziția Y de start pentru rândul următor
        cell_y_top_accumulator -= current_row_height;
    }
}

// IN XhtmlToPdfConverter.cpp

void PdfConverter_old::renderCellContents(
    const TableRenderData& tableData,
    RenderingContext_old& currentContext)
{
    double cellPadding = tableData.cellPadding;

    size_t cellDrawDataIndex = 0;
    double cell_y_top_accumulator = tableData.rowStart_y;

    for (size_t r = 0; r < tableData.rows.size(); ++r) {
        double current_row_height = tableData.rowHeights[r];
        double initial_row_y = cell_y_top_accumulator;
        size_t cell_index_in_row = 0;

        for (int c_logical = 0; c_logical < tableData.numColumns; ) {

            // ... (VERIFICARE ROWSPAN & Evitare Crash-uri - neschimbată) ...
            if (r < tableData.occupiedGrid.size() && c_logical < tableData.occupiedGrid[r].size() &&
                tableData.occupiedGrid[r][c_logical] > 0)
            {
                c_logical++;
                continue;
            }
            if (cell_index_in_row >= tableData.rows[r].size() || cellDrawDataIndex >= tableData.cellDrawData.size()) {
                c_logical++;
                cell_index_in_row++;
                cellDrawDataIndex++;
                continue;
            }

            const auto& entry = tableData.cellDrawData[cellDrawDataIndex];
            const CellContentState& state = entry.first;
            double measured_final_y = entry.second;

            const CellMetadata& cellMeta = tableData.rows[r][cell_index_in_row];
            int colSpan = cellMeta.colSpan;
            int rowSpan = cellMeta.rowSpan;

            // --- CALCUL GEOMETRIE X (POZIȚIE ȘI LĂȚIME) ---
            double currentCellWidth = 0.0;
            double current_cell_start_x = currentContext.margin_x;

            if (tableData.isFixedLayout) {
                for (int i = 0; i < c_logical; ++i) {
                    if (i < tableData.fixedColWidths.size()) {
                        current_cell_start_x += tableData.fixedColWidths[i];
                    }
                }
                for (int i = 0; i < colSpan; ++i) {
                    if (c_logical + i < tableData.fixedColWidths.size()) {
                        currentCellWidth += tableData.fixedColWidths[c_logical + i];
                    }
                }
            }
            else {
                double colLogicalWidth = tableData.colWidth;
                current_cell_start_x = currentContext.margin_x + (c_logical * colLogicalWidth);
                currentCellWidth = colLogicalWidth * colSpan;
            }

            // Setăm limitele X de conținut (cu padding inclus)
            double content_start_x = current_cell_start_x + cellPadding;
            double max_content_x = current_cell_start_x + currentCellWidth - cellPadding;

            // --- B & C. CALCUL GEOMETRIE Y ȘI ALINIERA VERTICALĂ (Logica neschimbată) ---
            double actual_cell_height = current_row_height;
            if (rowSpan > 1) {
                actual_cell_height = 0.0;
                for (int rs = 0; rs < rowSpan; ++rs) {
                    if (r + rs < tableData.rowHeights.size()) {
                        actual_cell_height += tableData.rowHeights[r + rs];
                    }
                }
            }

            std::wstring verticalAlign = state.context.vertical_align;
            double text_height_above_baseline = state.context.current_font_size * 0.8;
            double content_start_y_baseline = initial_row_y - cellPadding - text_height_above_baseline;
            double measured_content_height = content_start_y_baseline - measured_final_y;
            double available_content_height = actual_cell_height - (cellPadding * 2.0);
            double V_offset_to_apply = 0.0;

            double vertical_slack;
            if (available_content_height > measured_content_height) {
                vertical_slack = available_content_height - measured_content_height;
                if (verticalAlign == L"middle") {
                    V_offset_to_apply = vertical_slack / 2.0;
                }
                else if (verticalAlign == L"bottom") {
                    V_offset_to_apply = vertical_slack;
                }
            }
            double new_content_start_y_baseline = content_start_y_baseline - V_offset_to_apply;
            

            // =========================================================================
            // NOU: LOGARE ALINIERE VERTICALĂ
            // =========================================================================
            //LOG_DEBUG(L"[V-ALIGN] Cell (R:" + std::to_wstring(r) + L", C:" + std::to_wstring(c_logical) + L")");
            //LOG_DEBUG(L"  - CSS V-Align: " + verticalAlign);
            //LOG_DEBUG(L"  - Cell Height (Actual): " + std::to_wstring(actual_cell_height));
            //LOG_DEBUG(L"  - Content Height (Measured): " + std::to_wstring(measured_content_height));
            //LOG_DEBUG(L"  - Available Height: " + std::to_wstring(available_content_height));
            //LOG_DEBUG(L"  - Vertical Slack: " + std::to_wstring(vertical_slack));
            //LOG_DEBUG(L"  - V-Offset Applied: " + std::to_wstring(V_offset_to_apply));
            //LOG_DEBUG(L"  - Initial Y Baseline: " + std::to_wstring(content_start_y_baseline));
            //LOG_DEBUG(L"  - Final Cursor Y Baseline: " + std::to_wstring(new_content_start_y_baseline));
            // =========================================================================

            // --- D. DESENARE CONȚINUT ---

            // 1. SETARE CONTEXT
            RenderingContext_old originalContext = m_context;
            m_pdfWriter.saveGraphicState();

            m_context = state.context;
            m_context.cursor_y = new_content_start_y_baseline;

            // CORECȚII CRITICE X (Asigură că funcția processContentAsText știe unde începe și se termină)
            m_context.margin_x = content_start_x;
            m_context.cursor_x = content_start_x;
            m_context.page_width = max_content_x;

            m_context.inside_table_cell = true;
            m_context.current_line_has_content = false;
            m_context.last_item_was_space = false;

            // 2. RE-EXECUTĂM LOGICA DE RANDARE
            for (const auto& child : state.cellElement->subElements) {
                if (child.tagName == L"#text") {
                    // processContentAsText va folosi m_context.margin_x și m_context.page_width
                    //processContentAsText(child.content, m_context, max_content_x);
                    processContentAsText(child, m_context, max_content_x);
                }
                else {
                    // Logica pentru tabele imbricate și alte elemente...
                    m_context.is_measuring_table_content = true;
                    processNodeRecursive(child);
                    m_context.is_measuring_table_content = false;
                }
            }

            // 3. RESTAURARE CONTEXT
            m_pdfWriter.restoreGraphicState();
            m_context = originalContext;

            // 4. Avansare Cursori de iterație
            c_logical += colSpan;
            cell_index_in_row++;
            cellDrawDataIndex++;
        }

        // Actualizăm poziția Y de start pentru rândul următor
        cell_y_top_accumulator -= current_row_height;
    }
}

void PdfConverter_old::processNodeRecursive(const XhtmlElement& element) {
    // ----------------------------------------------------------------------
    // 0. SKIP elemente nefolositoare
    // ----------------------------------------------------------------------
    if (element.tagName == L"style" || element.tagName == L"script" ||
        element.tagName == L"head" || element.tagName == L"title") {
        return;
    }

    // 1. PUSH: SALVEAZĂ STAREA COMPLETĂ A PĂRINTELUI
    RenderingContext_old savedContext = m_context;
    m_context.depth++;

    // Salvează culoarea de fundal a părintelui (pentru restaurare forțată/inline)
    ColorRgb original_parent_bg_color = savedContext.current_background_color;

    bool isBlock = isBlockElement(element.tagName);

    // Definirea explicită a elementelor care primesc spațiu suplimentar (marjă)
    bool needsExtraSpacing = (element.tagName == L"p" || element.tagName == L"h1" ||
        element.tagName == L"h2" || element.tagName == L"h3" ||
        element.tagName == L"div" || element.tagName == L"table" || element.tagName == L"img"); // Am adăugat img

    // ----------------------------------------------------------------------
    // 2. LOGICA BLOCK START (Începe pe o linie nouă)
    // ----------------------------------------------------------------------
    if (isBlock && !m_context.is_measuring_table_content && !m_context.inside_table_cell) {

        // Asigură încheierea liniei anterioare, dacă nu era goală
        if (m_context.current_line_has_content) {
            // Mută Y pentru a crea spațiu (simulează line-height)
            m_context.cursor_y -= m_context.current_font_size * 1.2;
        }

        m_context.cursor_x = m_context.margin_x; // Reset cursor X la marginea de start
        m_context.current_line_has_content = false;
        m_context.last_item_was_space = false;

        // CORECȚIE: S-a eliminat marja de sus aici pentru a nu se dubla
        // spațiul între două elemente block. Marja se aplică doar la final (Pasul 5).
    }


    // 3. APPLY: APLICĂ STILURILE ELEMENTULUI
    applyCssToContext(element.tagName, element.attributes);

    // ----------------------------------------------------------------------
    // 4. PROCESEAZĂ CONȚINUTUL ȘI COPIII
    // ----------------------------------------------------------------------
    if (element.tagName == L"h1") {
        processH1(element, m_context);
    }
    else if (element.tagName == L"p") {
        processP(element, m_context);
    }
    else if (element.tagName == L"span") {
        processSpan(element, m_context);
    }
    else if (element.tagName == L"table") {
        processTable(element, m_context);
    }
    else if (element.tagName == L"img") {
        processImg(element, m_context);
    }
    else if (element.tagName == L"br") {
        processLineBreak(m_context);
    }
    else if (isBlockElement(element.tagName)) {
        processGenericBlock(element, m_context);
    }
    else {
        processInline(element, m_context);
    }

    // ----------------------------------------------------------------------
    // 5. LOGICA BLOCK END (Reguli de spațiere și curățare)
    // ----------------------------------------------------------------------

    // CORECȚIE FUNDAL INLINE: Resetează fundalul la cel al părintelui,
    // pentru a preveni moștenirea culorii de fundal inline (ex: <span>)
    if (!isBlock) {
        m_context.current_background_color = original_parent_bg_color;
        LOG_DEBUG(L"[CONTEXT RESET INLINE] BG resetat la parinte inainte de POP.");
    }

    if (isBlock && !m_context.is_measuring_table_content && !m_context.inside_table_cell) {

        // Asigură întreruperea de linie după block
        if (m_context.current_line_has_content) {
            processLineBreak(m_context);
        }

        // Aplica marja de jos (pentru a evita dublarea spațiului între block-uri)
        if (needsExtraSpacing) {
            m_context.cursor_y -= 5.0;
        }

        // Cursorul X este deja resetat la începutul Pasului 6
    }

    // ----------------------------------------------------------------------
    // 6. POP: RESTAURARE STIL (ȘI TRANSFER POZIȚIE)
    // ----------------------------------------------------------------------

    // a) Salvează starea finală a cursorului și a liniei curente
    double final_cursor_x = m_context.cursor_x;
    double final_cursor_y = m_context.cursor_y;
    bool final_line_has_content = m_context.current_line_has_content;
    bool final_last_item_was_space = m_context.last_item_was_space; // (Opțional, dar bun)

    // b) RESTAURARE STIL COMPLET: Resetează m_context la starea salvată
    m_context = savedContext;
    m_context.depth--;

    // c) CORECȚIE CRITICĂ FUNDAL (Siguranță):
    m_context.current_background_color = original_parent_bg_color;
    LOG_DEBUG(L"[CONTEXT BG FINAL] Fundal restaurat la starea inițială.");

    // d) TRANSFER POZIȚIE: Aplică modificările de poziție noului (restauratului) m_context
    m_context.cursor_x = final_cursor_x;
    m_context.cursor_y = final_cursor_y;
    m_context.current_line_has_content = final_line_has_content;
    m_context.last_item_was_space = final_last_item_was_space;
}


// PdfConverter.cpp - Metodă privată

double PdfConverter_old::convertCssLengthToPt(const std::wstring& cssValue) const {
    if (cssValue.empty()) return 0.0;

    // Simplu 'trim' (eliminare spații albe)
    std::wstring trimmed = cssValue;
    trimmed.erase(0, trimmed.find_first_not_of(L" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(L" \t\n\r") + 1);

    // În cazul proprietăților complexe precum 'padding: 5px 10px', 
    // această metodă simplă ar trebui să fie îmbunătățită pentru a parsa multiple valori.
    // Presupunem că folosiți o singură valoare (ex: "5px").

    try {
        if (trimmed.size() >= 2) {
            std::wstring unit = trimmed.substr(trimmed.size() - 2);
            std::wstring val_str = trimmed.substr(0, trimmed.size() - 2);

            double value = std::stod(val_str);

            if (unit == L"px") {
                // 1px = 0.75pt (standard 96 DPI)
                return value * 0.75;
            }
            if (unit == L"mm") {
                // 1mm = 72 / 25.4 pt (~2.835 pt)
                return value * (72.0 / 25.4);
            }
            if (unit == L"pt") {
                return value;
            }
        }
    }
    catch (...) {
        // În caz de eroare de parsare
        return 0.0;
    }
    return 0.0;
}


double PdfConverter_old::getCellPaddingFromCss() const {
    // 1. Interogăm CssDefinition pentru proprietatea 'padding' a selectorului 'td'
    
    std::wstring paddingValue = m_xhtml.getStyles().getPropertyValue(L"td", L"padding");
   
    if (paddingValue.empty()) {
        // Daca nu e setat, returnăm 0 (sau o valoare implicită minimă, ex: 1.0)
        return 0.0;
    }

    // 2. Convertim valoarea extrasă (ex: "5px") în puncte
    // NOTĂ: Dacă padding-ul este definit cu mai multe valori (ex: "5px 10px"),
    // această logică trebuie îmbunătățită pentru a le parsa pe toate 4 și a returna padding-top.
    // Pentru 'padding: 5px;' va returna valoarea corectă, care este folosită uniform.
    return convertCssLengthToPt(paddingValue);
}


void PdfConverter_old::processLineBreak(RenderingContext_old& currentContext) {

    // 1. Calculăm înălțimea liniei curente.
    // Folosim un factor de 1.2 ca la finalul unui bloc.
    double line_height = currentContext.current_font_size * 1.2;

    // 2. Mutăm cursorul Y în jos.
    currentContext.cursor_y -= line_height;

    // 3. Resetăm cursorul X la marginea de conținut (începutul noii linii).
    currentContext.cursor_x = currentContext.margin_x;

    // 4. Resetăm flag-urile de stare.
    currentContext.current_line_has_content = false;
    currentContext.last_item_was_space = false;

    // Notă: Nu redesenăm nimic, doar mutăm poziția.
    //LOG_DEBUG(L"[BR] Line break forțat. Cursor Y nou: " + std::to_wstring(currentContext.cursor_y));
}


void PdfConverter_old::printTableGrid(const TableRenderData& tableData)
{
    if (tableData.rows.empty() || tableData.numColumns == 0) {
        std::wcout << L"Tabelul nu conține rânduri sau coloane logice (numColumns=0).\n";
        return;
    }

    std::wcout << L"\n--- Harta Logică a Tabelei (Rânduri: " << tableData.rows.size()
        << L", Coloane Logice: " << tableData.numColumns << L") ---\n";
    std::wcout << L"Legenda: [1] = Start celulă reală (td/th), [0] = Celulă fantomă (Colspan sau Rowspan)\n";

    // Antet pentru a alinia vizual coloanele
    std::wcout << L"Rând/Coloană: ";
    for (int c = 0; c < tableData.numColumns; ++c) {
        std::wcout << L"[" << c % 10 << L"]";
    }
    std::wcout << L"\n";

    // Iterație pentru a construi și afișa grila
    for (size_t r = 0; r < tableData.rows.size(); ++r) {
        std::wcout << L"Rând " << r << L":       ";
        int c_logical = 0;
        size_t cell_index_in_row = 0;

        while (c_logical < tableData.numColumns) {

            // 1. Verificare Rowspan Ghost Cell (ocupată de o celulă din rândul superior)
            bool is_occupied_by_rowspan = (r < tableData.occupiedGrid.size() &&
                c_logical < tableData.occupiedGrid[r].size() &&
                tableData.occupiedGrid[r][c_logical] > 0);

            if (is_occupied_by_rowspan) {
                // Celulă fantomă (Rowspan)
                std::wcout << L"[0]";
                c_logical++;
            }
            // 2. Verificare Celulă Reală (un slot liber la care putem plasa o celulă reală)
            else if (cell_index_in_row < tableData.rows[r].size()) {

                const CellMetadata& cellMeta = tableData.rows[r][cell_index_in_row];
                int colSpan = cellMeta.colSpan;

                // Prima coloană logică a celulei reale (Marcaj 1)
                std::wcout << L"[1]";

                // Coloanele logice acoperite de colspan (Marcaj 0)
                for (int c_span = 1; c_span < colSpan; ++c_span) {
                    if (c_logical + c_span < tableData.numColumns) {
                        std::wcout << L"[0]";
                    }
                }

                // Avansăm
                c_logical += colSpan;
                cell_index_in_row++;
            }
            // 3. Spațiu gol neocupat de nicio celulă reală sau rowspan (ar trebui să fie rar)
            else {
                std::wcout << L"[0]";
                c_logical++;
            }
        }
        std::wcout << L"\n";
    }
    std::wcout << L"----------------------------------------------------\n";
}


ColorRgb PdfConverter_old::parseCssColorToRgb(const std::wstring& css_color_val) {
    if (css_color_val.empty() || to_lower(css_color_val) == L"transparent") {
        return { -1.0, -1.0, -1.0 }; // Transparent
    }

    std::wstring color = to_lower(css_color_val);

    if (color.front() == L'#' && color.length() == 7) {
        int r = std::stoi(color.substr(1, 2), nullptr, 16);
        int g = std::stoi(color.substr(3, 2), nullptr, 16);
        int b = std::stoi(color.substr(5, 2), nullptr, 16);
        return { r / 255.0, g / 255.0, b / 255.0 };
    }

    // Sintaxa rgb(r, g, b)
    if (color.find(L"rgb(") == 0 && color.back() == L')') {
        size_t p1 = color.find(L'(');
        size_t p2 = color.find(L')');
        std::wstring inner = color.substr(p1 + 1, p2 - p1 - 1);

        std::wistringstream ss(inner);
        std::wstring token;
        std::vector<int> values;

        while (std::getline(ss, token, L',')) {
            values.push_back(std::stoi(token));
        }

        if (values.size() == 3) {
            return { values[0] / 255.0, values[1] / 255.0, values[2] / 255.0 };
        }
    }

    // Culori CSS standard
    if (color == L"black")   return { 0.0, 0.0, 0.0 };
    if (color == L"white")   return { 1.0, 1.0, 1.0 };
    if (color == L"red")     return { 1.0, 0.0, 0.0 };
    if (color == L"green")   return { 0.0, 1.0, 0.0 };
    if (color == L"blue")    return { 0.0, 0.0, 1.0 };
    if (color == L"yellow")  return { 1.0, 1.0, 0.0 };
    if (color == L"gray")    return { 0.5, 0.5, 0.5 };
    if (color == L"cyan")    return { 0.0, 1.0, 1.0 };
    if (color == L"magenta") return { 1.0, 0.0, 1.0 };
    if (color == L"orange")  return { 1.0, 0.65, 0.0 };
    if (color == L"purple")  return { 0.5, 0.0, 0.5 };
    if (color == L"brown")   return { 0.6, 0.4, 0.2 };

    return { 0.0, 0.0, 0.0 }; // fallback la negru
}

// Metodă ajutătoare pentru conversia unităților (presupunând că există)
// De exemplu: double PdfConverter::mmToPoints(double mm) { return mm * 2.83465; }

void PdfConverter_old::processImg(const XhtmlElement& element, RenderingContext_old& currentContext) {
    auto src_attr = element.attributes.find(L"src");
    if (src_attr == element.attributes.end() || src_attr->second.empty()) {
        LOG_WARNING(L"<img> tag without src attribute. Skipping.");
        return;
    }
    std::wstring src_value = src_attr->second;

    // 1. Mută cursorul Y pentru a începe imaginea (tratată ca un element block)
    if (currentContext.current_line_has_content) {
        processLineBreak(currentContext); // Forțează o linie nouă înainte de imagine
    }

    // 2. Extrage și convertește dimensiunile
    double final_width_pts = 0.0;
    double final_height_pts = 0.0;

    // ... (Logica de extragere a dimensiunilor din HTML/CSS rămâne aceeași) ...

    auto width_attr = element.attributes.find(L"width");
    auto height_attr = element.attributes.find(L"height");

    if (width_attr != element.attributes.end()) {
        final_width_pts = convertCssLengthToPt(width_attr->second);
        LOG_DEBUG(L"Dimensiuni preluate din atributul HTML width: " + width_attr->second);
    }
    if (height_attr != element.attributes.end()) {
        final_height_pts = convertCssLengthToPt(height_attr->second);
        LOG_DEBUG(L"Dimensiuni preluate din atributul HTML height: " + height_attr->second);
    }

    // Fallback: 41.1mm și 14.6mm pentru clasa .logo (presupunând că sunt folosite)
    if (final_width_pts == 0.0) {
        final_width_pts = convertCssLengthToPt(L"41.1mm");
        LOG_DEBUG(L"Dimensiune latime setata din CSS default: 41.1mm");
    }
    if (final_height_pts == 0.0) {
        final_height_pts = convertCssLengthToPt(L"14.6mm");
        LOG_DEBUG(L"Dimensiune inaltime setata din CSS default: 14.6mm");
    }

    std::wstringstream dim_ss;
    dim_ss << L"[DIMENSIUNI FINALE] Latime: " << final_width_pts << L" pt, Inaltime: " << final_height_pts << L" pt.";
    LOG_INFO(dim_ss.str());

    if (final_width_pts <= 0.0 || final_height_pts <= 0.0) {
        LOG_WARNING(L"Image dimensions could not be resolved. Skipping.");
        return;
    }

    // --- 3. Ajustarea Poziției Y CORECTATĂ ---

    // a. Coordonata Y de START (sus) a imaginii:
    double image_y_top = currentContext.cursor_y;

    // b. Adaugă o mică marjă deasupra (spațierea dintre elemente)
    double image_margin_top = currentContext.current_font_size * 0.5;
    image_y_top -= image_margin_top;

    // c. Calculează Coordonata Y de JOS a imaginii (colțul stânga jos, necesar pentru DrawImage)
    double image_y_bottom = image_y_top - final_height_pts;

    // d. Coordonata de desenare:
    double image_x = currentContext.cursor_x; // Plasează la marginea stângă
    double image_y = image_y_bottom; // <--- CORECT! Folosim baza reală a imaginii pentru desenare.

    // e. Aplică marja de jos și setează noul cursor Y pentru conținutul următor
    double image_margin_bottom = currentContext.current_font_size;// *0.5;
    currentContext.cursor_y = image_y_bottom - image_margin_bottom; // Cursorul Y merge SUB marja de jos

    // LOGARE DE DIAGNOSTIC PENTRU POZIȚIE ȘI DIMENSIUNI:
    std::wstringstream pos_ss;
    pos_ss << L"[IMAGE POS] Desenare la X:" << image_x
        << L", Y (Bottom Draw):" << image_y // Acesta este Y-ul folosit pentru desenare
        << L", W:" << final_width_pts
        << L", H:" << final_height_pts
        << L" pt. | Cursor Y ulterior (pentru text): " << currentContext.cursor_y;
    LOG_DEBUG(pos_ss.str());

    bool image_added_successfully = false;

    // 4. Gestionarea sursei (Base64 vs. Cale Externă)
    // ... (Logica Base64 și File rămâne aceeași, dar folosește 'image_y' corect) ...

    // Verificare Base64 (Data URL)
    if (src_value.rfind(L"data:image/", 0) == 0) {
        LOG_INFO(L"Detectata sursa Base64 (Data URL).");
        size_t base64_pos = src_value.find(L";base64,");
        if (base64_pos != std::wstring::npos) {
            std::wstring base64_data = src_value.substr(base64_pos + 8);
            std::string image_data_binary = decodeBase64(base64_data);

            LOG_INFO(L"APEL: m_pdfWriter.addImage(date_binare, W:" + std::to_wstring(final_width_pts) + L", H:" + std::to_wstring(final_height_pts) + L", X:" + std::to_wstring(image_x) + L", Y:" + std::to_wstring(image_y) + L")");
            image_added_successfully = m_pdfWriter.addImage(
                image_data_binary, // Date binare decodate
                final_width_pts,
                final_height_pts,
                image_x,
                image_y // FOLOSEȘTE image_y_bottom
            );

            if (image_added_successfully) {
                LOG_INFO(L"Imagine Base64 inserata cu succes.");
            }
            else {
                LOG_ERROR(L"Esec la inserarea imaginii Base64.");
            }
        }
        else {
            LOG_WARNING(L"Data URL found but Base64 encoding not detected.");
        }
    }
    else {
        // Cale Externă (ex: imgs/romatsa2.jpg)
        std::string file_path_utf8 = wstring_to_utf8(src_value);

        LOG_INFO(L"APEL: m_pdfWriter.addImageFromFile(cale, W:" + std::to_wstring(final_width_pts) + L", H:" + std::to_wstring(final_height_pts) + L", X:" + std::to_wstring(image_x) + L", Y:" + std::to_wstring(image_y) + L")");
        image_added_successfully = m_pdfWriter.addImageFromFile(
            file_path_utf8,      // 1. Calea (std::string)
            final_width_pts,     // 2. Lățimea (double)
            final_height_pts,    // 3. Înălțimea (double)
            image_x,             // 4. Coordonata X (double)
            image_y              // 5. Coordonata Y (double)
        );

        if (!image_added_successfully) {
            LOG_ERROR(L"Esec la inserarea imaginii din fisier: " + src_value);
        }
    }

    // 5. Finalizare: Resetarea stării liniei
    if (image_added_successfully) {
        currentContext.cursor_x = currentContext.margin_x;
        currentContext.current_line_has_content = false;
        currentContext.last_item_was_space = false;
    }
}

// Setul de caractere Base64: 64 de caractere (A-Z, a-z, 0-9, +, /) și caracterul de padding (=)
const std::string BASE64_CHARS =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

// Funcție utilitară pentru a obține valoarea (indexul) unui caracter Base64.
// Returnează -1 pentru caractere ne-Base64 (inclusiv padding-ul, care e tratat separat).
inline int base64_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1; // Caracter invalid sau padding
}

// Funcția principală de decodare Base64
// Input: std::wstring (șirul Base64 din src)
// Output: std::string (date binare brute)
std::string PdfConverter_old::decodeBase64(const std::wstring& encoded_wstring) {
    // Conversia wstring (UTF-16/32) la string (char) - necesar pentru date binare și performanță
    std::string encoded_string;
    for (wchar_t wc : encoded_wstring) {
        // Presupunem că șirul Base64 conține doar caractere ASCII (ceea ce este adevărat)
        encoded_string.push_back(static_cast<char>(wc));
    }

    int in_len = encoded_string.length();
    if (in_len == 0) return "";

    // Alocare memorie pentru rezultat. Max 3 bytes pentru fiecare 4 caractere.
    std::string ret;
    ret.reserve(in_len * 3 / 4);

    int i = 0;
    while (i < in_len) {
        // 1. Colectează 4 caractere Base64 (sau padding)
        char char_array_4[4] = { 0, 0, 0, 0 };
        int j = 0;
        int k = i;

        while (j < 4 && k < in_len) {
            char c = encoded_string[k++];
            if (c == '=') {
                char_array_4[j++] = c;
            }
            else {
                int val = base64_value(c);
                if (val != -1) {
                    char_array_4[j++] = c;
                }
                // Ignoră caracterele whitespace sau invalide din șir
            }
        }

        // 2. Obține valorile index (0-63)
        int in[4];
        for (int idx = 0; idx < 4; ++idx) {
            if (char_array_4[idx] == '=') {
                in[idx] = 0; // Padding-ul este tratat cu valoarea 0 temporar
            }
            else {
                in[idx] = base64_value(char_array_4[idx]);
            }
        }

        // 3. Reasamblează cei 3 octeți originali din cele 4 valori de 6 biți
        unsigned char char_array_3[3];
        char_array_3[0] = (in[0] << 2) + ((in[1] & 0x30) >> 4);
        char_array_3[1] = ((in[1] & 0xf) << 4) + ((in[2] & 0x3c) >> 2);
        char_array_3[2] = ((in[2] & 0x3) << 6) + in[3];

        // 4. Adaugă octeții rezultatului, excluzând pe cei creați din padding

        // Câte octeți valizi au fost creați:
        int output_bytes = 3;
        if (char_array_4[3] == '=') output_bytes = 2;
        if (char_array_4[2] == '=') output_bytes = 1;

        for (int idx = 0; idx < output_bytes; ++idx) {
            ret += char_array_3[idx];
        }

        i = k; // Treci la următorul bloc de 4 caractere
    }

    return ret;
}

void PdfConverter_old::calculateFixedRowHeightsAndCellContexts(
    TableRenderData& tableData,
    RenderingContext_old& currentContext)
{
    // Inițializări
    double rowStart_y = tableData.rowStart_y;
    // NOTA: Recomandarea este ca getCellPaddingFromCss() să citească CSS-ul td/th
    double cellPadding = getCellPaddingFromCss();
    double text_height_above_baseline = currentContext.current_font_size * 0.8;
    // Folosim o înălțime de linie standard pentru a forța înălțimea minimă a celulelor goale/single-line
    double calculated_line_height = currentContext.current_font_size * 1.2;

    // Obține acces la definițiile CSS
    const CssDefinition& css = m_xhtml.getStyles(); // Folosim referință constantă

    // Colectăm informații pentru celulele cu rowspan.
    std::vector<std::tuple<size_t, double, int>> spanningCellInfo;

    // PASS 1: MĂSURAREA CONȚINUTULUI ȘI APLICAREA ÎNĂLȚIMII FIXE
    for (size_t r = 0; r < tableData.rows.size(); ++r) {
        double initial_row_y = rowStart_y;
        size_t cell_index_in_row = 0;

        // *******************************************************************
        // LOGICĂ: CALCULAREA ÎNĂLȚIMII RÂNDULUI (H_dynamic_row)
        // *******************************************************************
        const XhtmlElement* trElement = tableData.rowElements[r];
        double H_dynamic_row = 0.0;

        // 1. Încercare de citire a înălțimii specifice din CSS (Tag/Clasă)
        if (trElement->attributes.count(L"class")) {
            std::wstring rowClasses = trElement->attributes.at(L"class");
            for (const auto& className : wexplode(rowClasses, L' ')) { // Presupunem wexplode
                std::wstring selector = L"tr." + className;
                std::wstring heightValue = css.getPropertyValue(selector, L"height");
                H_dynamic_row = convertCssLengthToPt(heightValue);
                if (H_dynamic_row > 0.0) break;
            }
        }
        if (H_dynamic_row == 0.0) {
            std::wstring heightValue = css.getPropertyValue(L"tr", L"height");
            H_dynamic_row = convertCssLengthToPt(heightValue);
        }

        // 2. Fallback la valoarea minimă sigură
        if (H_dynamic_row == 0.0) {
            // Folosim înălțimea minimă necesară pentru o linie de text + padding
            H_dynamic_row = calculated_line_height + (cellPadding * 2.0);
        }
        // *******************************************************************

        for (int c_logical = 0; c_logical < tableData.numColumns; ) {

            // 1. VERIFICARE ROWSPAN
            if (r < tableData.occupiedGrid.size() && c_logical < tableData.occupiedGrid[r].size() &&
                tableData.occupiedGrid[r][c_logical] > 0)
            {
                c_logical++;
                continue;
            }

            if (cell_index_in_row >= tableData.rows[r].size()) {
                c_logical++;
                continue;
            }

            // 3. PROCESARE CELULĂ REALĂ
            const CellMetadata& cellMeta = tableData.rows[r][cell_index_in_row];
            const XhtmlElement* cellElement = cellMeta.cellElement;
            int colSpan = cellMeta.colSpan;
            int rowSpan = cellMeta.rowSpan;

            // Calculul lățimii și poziției X (neschimbat)
            double currentCellWidth = 0.0;
            for (int i = 0; i < colSpan; ++i) {
                if (c_logical + i < tableData.fixedColWidths.size()) {
                    currentCellWidth += tableData.fixedColWidths[c_logical + i];
                }
            }
            double current_cell_start_x = currentContext.margin_x;
            for (int i = 0; i < c_logical; ++i) {
                if (i < tableData.fixedColWidths.size()) {
                    current_cell_start_x += tableData.fixedColWidths[i];
                }
            }

            // A. PREGĂTIRE CONTEXT PENTRU MĂSURARE
            RenderingContext_old cellContext = currentContext;
            RenderingContext_old originalContext = m_context; // Salvează m_context global

            // Mutăm m_context global la cel al celulei, resetând starea
            m_context = cellContext;
            m_context.inside_table_cell = true;
            m_context.is_measuring_table_content = true; // Setăm pentru faza de măsurare
            m_context.current_line_has_content = false;

            // Setăm constrângerile X și poziția Y inițială (baseline)
            m_context.margin_x = current_cell_start_x + cellPadding;
            double max_content_x = current_cell_start_x + currentCellWidth - cellPadding;
            m_context.page_width = max_content_x;

            double text_baseline_y_start = initial_row_y - cellPadding - text_height_above_baseline;
            m_context.cursor_y = text_baseline_y_start;
            m_context.cursor_x = m_context.margin_x;

            // Aplică CSS-ul CELULEI (Actualizează fontul, text-align, și vertical_align)
            applyCssToContext(cellElement->tagName, cellElement->attributes);

            // Stocare stare inițială
            // Contextul stocat conține acum proprietatea vertical_align citită din CSS
            CellContentState state = { cellElement, m_context, max_content_x };

            // C. MĂSURARE CONȚINUT (Simulare randare)
            for (const auto& child : cellElement->subElements) {
                if (child.tagName == L"#text") {
                    // Asigură-te că processContentAsText setează current_line_has_content = true
                    processContentAsText(child, m_context, max_content_x);
                }
                else {
                    processNodeRecursive(child);
                }
            }

            // --------------------------------------------------------------------------
            // FIX CRITIC PENTRU V-ALIGN: CORECȚIA ÎNĂLȚIMII CONȚINUTULUI PE O SINGURĂ LINIE
            // --------------------------------------------------------------------------
            double cell_final_y_measured = m_context.cursor_y;

            // Dacă cursorul Y nu s-a mișcat de la poziția inițială (o singură linie)
            // și am procesat conținut (m_context.current_line_has_content este true)
            if (std::abs(cell_final_y_measured - text_baseline_y_start) < 0.01 && m_context.current_line_has_content) {

                // Forțează mișcarea cursorului în jos cu o înălțime de linie (line break)
                m_context.cursor_y -= calculated_line_height;
                m_context.current_line_has_content = false;

                // Re-actualizează Y-ul final măsurat
                cell_final_y_measured = m_context.cursor_y;
            }
            // --------------------------------------------------------------------------

            // D. CALCULUL ÎNĂLȚIMII CONȚINUTULUI MĂSURAT
            double H_content_needed = text_baseline_y_start - cell_final_y_measured;

            // E. ACTUALIZARE ÎNĂLȚIME RÂND (folosim celula care necesită cel mai mare spațiu)
            // Calculăm înălțimea totală necesară, incluzând padding-ul.
            double H_required_with_padding = H_content_needed + (cellPadding * 2.0);

            // Asigură că înălțimea rândului este cel puțin egală cu înălțimea necesară
            // pentru a cuprinde conținutul celulei actuale (dacă H_dynamic_row este mai mică)
            H_dynamic_row = std::max<double>(H_dynamic_row, H_required_with_padding);


            // Colectăm informații pentru celulele cu rowspan
            if (rowSpan > 1) {
                // Înălțimea necesară (măsurată) stocată în info
                spanningCellInfo.emplace_back(r, H_required_with_padding, rowSpan);
            }

            // Stocarea datelor pentru randarea ulterioară
            tableData.cellDrawData.push_back({ state, cell_final_y_measured });
            m_context = originalContext; // Restaurarea m_context-ului

            // F. AVANSARE CURSORI
            c_logical += colSpan;
            cell_index_in_row++;
        }

        // G. CALCUL FINAL ÎNĂLȚIME RÂND: FOLOSIM ÎNĂLȚIMEA MAXIMĂ DETERMINATĂ!
        double actual_row_height = H_dynamic_row;

        // Actualizare rowStart_y și stocare înălțime
        rowStart_y = initial_row_y - actual_row_height;
        tableData.rowHeights.push_back(actual_row_height);
    }

    // PASS 2: AJUSTARE ROWSPAN (Verificare și Consolidare) - Neschimbat
    for (const auto& info : spanningCellInfo) {
        size_t start_r = std::get<0>(info);
        int rowSpan = std::get<2>(info);
        double H_content_required = std::get<1>(info);

        // Sumăm înălțimea rândurilor acoperite de rowspan
        double combined_current_height = 0.0;
        for (int k = 0; k < rowSpan; ++k) {
            size_t current_r = start_r + k;
            if (current_r < tableData.rowHeights.size()) {
                combined_current_height += tableData.rowHeights[current_r];
            }
        }

        // Dacă înălțimea necesară (măsurată) este mai mare decât înălțimea combinată alocată,
        // mărim ultimul rând pentru a cuprinde conținutul.
        if (H_content_required > combined_current_height) {
            size_t last_r = start_r + rowSpan - 1;
            if (last_r < tableData.rowHeights.size()) {
                double diff = H_content_required - combined_current_height;
                tableData.rowHeights[last_r] += diff; // Mărim ultimul rând
            }
        }
    }

    // RE-CALCULARE finală tableData.max_final_y_global
    rowStart_y = tableData.rowStart_y;
    for (double h : tableData.rowHeights) {
        rowStart_y -= h;
    }
    tableData.max_final_y_global = rowStart_y;
}