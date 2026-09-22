#pragma once

#include<string>
#include<map>
#include "ConvertUtils.hpp"
#include "ConsoleManager.hpp"

/*
//Stiluri vizuale:
- background-color: culoarea de fundal (background-color: lightblue)
- color: culoarea textului (color: black)
- border: conturul (border: 1px solid gray)
- border-color: conturul (border: 1px solid gray)
//Dimensiuni si spatiere:
- width, height: dimensiuni (width: 300px)
- padding: spațiu interior (padding: 20px)
- margin: spațiu exterior (margin: 10px)
- max-width, min-height: limite de dimensiuni
//Pozitionare si aliniere
- position: static, relative, absolute, fixed
- top, left, right, bottom: poziționare
- display: block, inline-block, none
- text-align: alinierea textului (text-align: center)
- vertical-align: aliniere verticală
//Fonturi și text
- font-family: tipul fontului (font-family: Arial)
- font-size: mărimea textului (font-size: 16px)
- font-weight: grosimea (font-weight: bold)
- font-style: stilul fontului (normal, italic)
- line-height: înălțimea liniei
- letter-spacing: Spațierea între litere
- word-spacing: Spațierea între cuvinte
- text-decoration: Subliniere, linie peste sau sub text

*/

struct FontStylePaths {
    std::string Regular;
    std::string Bold;
    std::string Italic;
    std::string BoldItalic;
};


struct BoxModel {
    double marginTop = 0.0;
    double marginRight = 0.0;
    double marginBottom = 0.0;
    double marginLeft = 0.0;
    double paddingTop = 0.0;
    double paddingRight = 0.0;
    double paddingBottom = 0.0;
    double paddingLeft = 0.0;

    // Border Width
    double borderTopWidth = 0.0;
    double borderRightWidth = 0.0;
    double borderBottomWidth = 0.0;
    double borderLeftWidth = 0.0;

    void print() const {
        LOG_DEBUG(L"      [Box] Margin: T:" + std::to_wstring(marginTop) + L", B:" + std::to_wstring(marginBottom) + L", L:" + std::to_wstring(marginLeft) + L", R:" + std::to_wstring(marginRight));
        LOG_DEBUG(L"      [Box] Padding: T:" + std::to_wstring(paddingTop) + L", B:" + std::to_wstring(paddingBottom) + L", L:" + std::to_wstring(paddingLeft) + L", R:" + std::to_wstring(paddingRight));
        LOG_DEBUG(L"      [Box] Border: T:" + std::to_wstring(borderTopWidth) + L", B:" + std::to_wstring(borderBottomWidth) + L", L:" + std::to_wstring(borderLeftWidth) + L", R:" + std::to_wstring(borderRightWidth));
    }
};

class Style {
public:
    // ==========================================================
    // ⭐ PROPRIETĂȚI (Membrul tău Style original)
    // ==========================================================
    std::wstring ruleName = L"";
    // 🎨 Culori și Borduri
    ColorRgb backgroundColor = { -1.0, -1.0, -1.0 }; // Transparent
    ColorRgb textColor = { 0.0, 0.0, 0.0 };         // Negru
    ColorRgb borderColor = { 0.0, 0.0, 0.0 };       // Negru
    std::wstring borderStyle = L"none";

    // 📏 Dimensiuni și Spațiere
    BoxModel boxModel;
    double width = -1.0;    // -1.0 = auto
    double height = -1.0;
    double maxWidth = -1.0;
    double minHeight = -1.0;

    // ⭐ SPAȚIERE RTF (Space Before \sb / Space After \sa în puncte)
    double spaceBeforePt = 0.0; // Corespunde la \sb
    double spaceAfterPt = 0.0;  // Corespunde la \sa

    // 🖼️ Flow și Poziționare
    std::wstring display = L"block";
    std::wstring position = L"static";

    double top = 0.0;
    double left = 0.0;
    double right = 0.0;
    double bottom = 0.0;


    std::wstring textAlign = L"left";
    std::wstring verticalAlign = L"top";

    // ✒️ Fonturi și Text
    std::wstring fontFamily = L"DefaultFont";
    double fontSize = 12.0;
    std::wstring fontWeight = L"normal"; // normal, lighter, bold, 900
    std::wstring fontStyle = L"normal"; // normal, italic, oblique
    double lineHeight = 1.2;
    double letterSpacing = 0.0;
    double wordSpacing = 0.0;
    std::wstring textDecoration = L"none"; // sau "underline", "line-through"

    //tabStops

    // ==========================================================
    // ⚙️ METODE ESENȚIALE (Logica)
    // ==========================================================

    Style() = default;
    Style(const Style& other) = default;
    Style& operator=(const Style & other) = default;
    /*
    Style(const Style& other) {
        // Copiere membrală automată (sau manuală, dacă ai pointeri, dar nu e cazul aici)
        *this = other;
    }
     */

    /**
     * @brief Verifică dacă display-ul curent este "block".
     */
    bool isBlock() const {
        return display == L"block" || display == L"list-item"; // S-ar putea să fie nevoie de mai multe
    }

    /**
     * @brief Calculează lățimea reală a conținutului, excluzând padding-ul, margin-ul și bordura.
     * @param availableWidth Lățimea spațiului disponibil (ex: lățimea paginii sau a elementului părinte).
     * @return Lățimea maximă rămasă pentru text/copii.
     */
    double getContentWidth(double availableWidth) const {
        if (width > 0.0) {
            availableWidth = width;
        }

        double horizontal_space = boxModel.paddingLeft + boxModel.paddingRight +
            boxModel.borderLeftWidth + boxModel.borderRightWidth +
            boxModel.marginLeft + boxModel.marginRight;

        return availableWidth - horizontal_space;
    }

   

    void setUpStyle(const std::map<std::wstring, std::wstring>& cssProperties);
    // ... (alte metode de validare sau calcul)

    void print() const;

    bool isFixed() const {
        return position == L"fixed";
    }

    /*
    void print() const {
        LOG_DEBUG(L"    --- STYLE DATA ---");
        LOG_DEBUG(L"      Display: " + display + L" | Position: " + position);
        LOG_DEBUG(L"      Size: W:" + std::to_wstring(width) + L", H:" + std::to_wstring(height));
        LOG_DEBUG(L"      Text Align: " + textAlign + L" | Font: " + fontFamily + L" " + std::to_wstring(fontSize) + L"pt");

        // Tipărește modelul de cutie (Box Model)
        boxModel.print();

        // Tipărește culori și borduri
        LOG_DEBUG(L"      Color: BG(" + std::to_wstring(backgroundColor.r) + L", " + std::to_wstring(backgroundColor.g) + L", " + std::to_wstring(backgroundColor.b) + L")");
        LOG_DEBUG(L"      Border Style: " + borderStyle + L" | Fixed: " + (isFixed() ? L"Yes" : L"No"));
        LOG_DEBUG(L"    --- END STYLE DATA ---");
    }
    */
};


static const std::map<std::wstring, std::wstring> HtmlDefaultDisplay = {
    {L"html", L"block"},
    {L"head", L"none"},
    {L"title", L"none"},
    {L"style", L"none"},
    {L"script", L"none"},
    {L"meta", L"none"},
    {L"link", L"none"},
    {L"body", L"block"},
    {L"div", L"block"},
    {L"p", L"block"},
    {L"h1", L"block"},
    {L"h2", L"block"},
    {L"h3", L"block"},
    {L"h4", L"block"},
    {L"h5", L"block"},
    {L"h6", L"block"},
    {L"ul", L"block"},
    {L"ol", L"block"},
    {L"li", L"list-item"},
    {L"table", L"table"},
    {L"thead", L"table-header-group"},
    {L"tbody", L"table-row-group"},
    {L"tfoot", L"table-footer-group"},
    {L"tr", L"table-row"},
    {L"td", L"table-cell"},
    {L"th", L"table-cell"},
    {L"form", L"block"},
    {L"input", L"inline-block"},
    {L"textarea", L"inline-block"},
    {L"button", L"inline-block"},
    {L"label", L"inline"},
    {L"span", L"inline"},
    {L"a", L"inline"},
    {L"img", L"inline"},
    {L"strong", L"inline"},
    {L"em", L"inline"},
    {L"b", L"inline"},
    {L"i", L"inline"},
    {L"small", L"inline"},
    {L"sup", L"inline"},
    {L"sub", L"inline"},
    {L"br", L"inline"},
    {L"hr", L"block"},
    {L"header", L"block"},
    {L"footer", L"block"},
    {L"section", L"block"},
    {L"article", L"block"},
    {L"aside", L"block"},
    {L"nav", L"block"},
    {L"figure", L"block"},
    {L"figcaption", L"block"},
    {L"main", L"block"},
    {L"canvas", L"inline"},
    {L"video", L"inline"},
    {L"audio", L"inline"},
    {L"details", L"block"},
    {L"summary", L"block"},
    {L"#text", L"inline"}
};

