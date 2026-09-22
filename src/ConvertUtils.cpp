#include "ConvertUtils.hpp"
#include "stringUtils.hpp"
#include "ConsoleManager.hpp"
#include <vector>
#include <cctype>
#include <algorithm>

ColorRgb ConvertUtils::parseCssColorToRgb(const std::wstring& css_color_val) {

   // return ColorRgb(0.0, 0.0, 0.0, 1.0);

    std::wstring color = to_lower(css_color_val);

    // 1. TRANSPARENT
    if (color.empty() || color == L"transparent") {
        // Transparent: A = 0.0
       // return { 0.0, 0.0, 0.0, 0.0 };
        return { -1.0, -1.0, -1.0, -1.0 };
    }

    // 2. SINTAXA HEXADECIMALĂ (#RRGGBB sau #RGB)
    if (color.front() == L'#') {

        // #RRGGBB (6 cifre)
        if (color.length() == 7) {
            int r = std::stoi(color.substr(1, 2), nullptr, 16);
            int g = std::stoi(color.substr(3, 2), nullptr, 16);
            int b = std::stoi(color.substr(5, 2), nullptr, 16);
            return { r / 255.0, g / 255.0, b / 255.0, 1.0 }; // Alpha = 1.0
        }

        // #RGB (3 cifre)
        else if (color.length() == 4) {
            int r = std::stoi(color.substr(1, 1) + color.substr(1, 1), nullptr, 16);
            int g = std::stoi(color.substr(2, 1) + color.substr(2, 1), nullptr, 16);
            int b = std::stoi(color.substr(3, 1) + color.substr(3, 1), nullptr, 16);
            return { r / 255.0, g / 255.0, b / 255.0, 1.0 }; // Alpha = 1.0
        }
    }

    // 3. SINTAXA FUNCȚIONALĂ (rgb() ȘI rgba())

    size_t p1 = color.find(L'(');
    size_t p2 = color.find(L')');

    if (p1 != std::wstring::npos && p2 != std::wstring::npos && p2 > p1) {
        std::wstring inner = color.substr(p1 + 1, p2 - p1 - 1);

        // Folosim wistringstream pentru a despărți pe virgulă
        std::wistringstream ss(inner);
        std::wstring token;
        std::vector<double> values;

        // Citește valorile (poate fi int sau double, mai ales pentru alpha)
        while (std::getline(ss, token, L',')) {
            // Ar trebui adăugat un trim() pe token, dar pentru simplitate...
            try {
                values.push_back(std::stod(token));
            }
            catch (...) {
                // Ignore conversion errors
            }
        }

        // rgb(r, g, b)
        if (color.find(L"rgb(") == 0 && values.size() == 3) {
            return { values[0] / 255.0, values[1] / 255.0, values[2] / 255.0, 1.0 }; // Alpha = 1.0
        }

        // ⭐ rgba(r, g, b, a) - CORECȚIA ESENȚIALĂ
        if (color.find(L"rgba(") == 0 && values.size() == 4) {
            // Primele 3 sunt pe 0-255, a patra este Alpha (de obicei 0.0 - 1.0)
            return { values[0] / 255.0, values[1] / 255.0, values[2] / 255.0, values[3] };
        }
    }

    // 4. CULORI NUMITE (Name Colors)

    // ⭐ CORECȚIE: Adaugă 1.0 (opacitate maximă) pentru toate culorile numite
    if (color == L"black")  return { 0.0, 0.0, 0.0, 1.0 };
    if (color == L"white")  return { 1.0, 1.0, 1.0, 1.0 };
    if (color == L"red")    return { 1.0, 0.0, 0.0, 1.0 };
    if (color == L"green")  return { 0.0, 1.0, 0.0, 1.0 };
    if (color == L"blue")   return { 0.0, 0.0, 1.0, 1.0 };
    if (color == L"yellow") return { 1.0, 1.0, 0.0, 1.0 };
    if (color == L"gray")   return { 0.5, 0.5, 0.5, 1.0 };
    if (color == L"cyan")   return { 0.0, 1.0, 1.0, 1.0 };
    if (color == L"magenta")    return { 1.0, 0.0, 1.0, 1.0 };
    if (color == L"orange") return { 1.0, 0.65, 0.0, 1.0 };
    if (color == L"purple") return { 0.5, 0.0, 0.5, 1.0 };
    if (color == L"brown")  return { 0.6, 0.4, 0.2, 1.0 };
    if (color == L"transparent")  return { -1., -1., -1., -1. };


    // 5. FALLBACK
    return { 0.0, 0.0, 0.0, 1.0 }; // Fallback la negru opac
}




// Notă: Semnătura corectă în .hpp trebuie să fie:
// static double convertCssLengthToPt(const std::wstring& cssValue, double currentFontSize);

double ConvertUtils::convertCssLengthToPt(const std::wstring& cssValue, double currentFontSize) {

   // return 5.0;

    // 1. Tratarea valorilor speciale
    if (cssValue.empty() || cssValue == L"auto" || cssValue == L"0") {
        return 0.0;
    }

    // Curățare (Trim)
    std::wstring trimmed = cssValue;
    trimmed.erase(0, trimmed.find_first_not_of(L" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(L" \t\n\r") + 1);

    if (trimmed.empty()) {
        return 0.0;
    }

    // 2. Parsare Valoare și Unitate (mai robust)
    try {
        // Caută prima literă (sau semnul care nu e cifră/punct/minus) pentru a marca începutul unității
        size_t unit_pos = trimmed.find_first_not_of(L"0123456789.-");

        std::wstring val_str;
        std::wstring unit;
        double value;

        if (unit_pos == std::wstring::npos) {
            // Cazul: Doar Număr (fără unitate). Pentru lungimi, tratăm ca PT.
            val_str = trimmed;
            unit = L"pt";
        }
        else {
            val_str = trimmed.substr(0, unit_pos);
            unit = trimmed.substr(unit_pos);
            // Transformăm în litere mici pentru comparație
            std::transform(unit.begin(), unit.end(), unit.begin(), ::towlower);
            unit = wstr_trim(unit); // <<--- LINIA NOUĂ!

        }

        if (val_str.empty()) return 0.0;
        value = std::stod(val_str);

        // 3. Conversia în Puncte (Pt)

        // --- Unități relative (necesită currentFontSize) ---
        if (unit == L"em") {
            // Corect: 1em este egal cu font-size-ul curent al elementului.
            return value * currentFontSize;
        }

        // --- Unități absolute (la fel ca înainte, plus in și pc) ---
        if (unit == L"pt") {
            return value;
        }
        if (unit == L"px") {
            // Standard 96 DPI: 1px = 0.75pt
            return value * 0.75;
        }
        if (unit == L"mm") {
            // 1mm = 72 / 25.4 pt (~2.835 pt)
            return value * (72.0 / 25.4);
        }
        if (unit == L"in") {
            // 1in = 72pt
            return value * 72.0;
        }
        if (unit == L"pc") {
            // 1pc (Pica) = 12pt
            return value * 12.0;
        }

        // --- Percentaje ---
        if (unit == L"%") {
            // NOTĂ: Procentajele pentru Box Model (margin, padding) depind de
            // lățimea/înălțimea PĂRINTELUI. Dacă nu ai acces la layout-ul părintelui
            // aici, trebuie să le tratezi ca 0.0 sau să treci lățimea/înălțimea părintelui ca argument.
            // Pentru simplitate inițială (CSS standard), tratăm ca 0.0 dacă nu ai lățimea părintelui.
            return 0.0;
        }

        // Dacă nu se potrivește nicio unitate cunoscută
        return 0.0;
    }
    catch (...) {
        // Capturare eroare de parsare
        return 0.0;
    }
}

void ConvertUtils::parseBorderShorthand(const std::wstring& value,
    std::wstring& out_style,
    double& out_width,
    ColorRgb& out_color,
    double currentFontSize)
{
    
    // NU RE-DECLARĂM width, style, color! Le folosim direct pe out_width, out_style, out_color.

    // Setăm inițial valorile de fallback, deși ele ar trebui să vină moștenite.
    out_width = 0.0;
    out_style = L"none";
    // out_color păstrează valoarea moștenită (sau o setăm la negru dacă vrei un fallback strict)

    std::wistringstream stream(value);
    std::wstring token;

    while (stream >> token) {
        // Detectăm lungimea (acum suportăm și 'em' corect!)
        // Folosim funcția cu 2 argumente
        if (token.find(L"px") != std::wstring::npos ||
            token.find(L"pt") != std::wstring::npos ||
            token.find(L"mm") != std::wstring::npos ||
            token.find(L"em") != std::wstring::npos)
        {
            out_width = convertCssLengthToPt(token, currentFontSize); // Folosim argumentul!
        }
        // Detectăm stilul de linie
        else if (token == L"solid" || token == L"dashed" || token == L"dotted" ||
            token == L"double" || token == L"none" || token == L"hidden")
        {
            out_style = token;
        }
        // Presupunem că restul este culoare (HEX, RGB, sau nume)
        else {
            out_color = parseCssColorToRgb(token);
        }
    }

    // NU mai aplicăm valorile pe boxModel.border... Asta o va face clasa Style.
}

//parseBoxShorthand(L"10px", boxModel.paddingTop, boxModel.paddingRight, boxModel.paddingBottom, boxModel.paddingLeft);
// → toate = 7.5pt
//parseBoxShorthand(L"10px 20px", ...);
// → top/bottom = 7.5pt, right/left = 15pt
//parseBoxShorthand(L"10px 20px 5px", ...);
// → top = 7.5pt, right/left = 15pt, bottom = 3.75pt
//parseBoxShorthand(L"10px 20px 5px 15px", ...);
// → top = 7.5pt, right = 15pt, bottom = 3.75pt, left = 11.25pt

void ConvertUtils::parseBoxShorthand(const std::wstring& value,
    double currentFontSize, // Folosim noul argument
    double& top, double& right,
    double& bottom, double& left) {

    //top = right = bottom = left = 0.0;



    std::vector<std::wstring> tokens;
    std::wistringstream stream(value);
    std::wstring token;

    // 1. Tokenizare (la fel ca înainte)
    while (stream >> token) {
        tokens.push_back(token);
    }

    // 2. Convertim fiecare valoare în puncte (Pt)
    std::vector<double> values;
    for (const auto& t : tokens) {
        // CORECȚIE CRITICĂ: Apelăm funcția corectă cu 2 argumente!
        values.push_back(convertCssLengthToPt(t, currentFontSize));
    }

    // 3. Aplicăm în funcție de numărul de valori (Logica CSS)
    // 
    if (values.size() == 1) {
        top = right = bottom = left = values[0];
    }
    else if (values.size() == 2) {
        top = bottom = values[0];
        right = left = values[1];
    }
    else if (values.size() == 3) {
        top = values[0];
        right = left = values[1];
        bottom = values[2];
    }
    else if (values.size() == 4) {
        top = values[0];
        right = values[1];
        bottom = values[2];
        left = values[3];
    }
    // Else (5+ token-uri) - ignorăm.
}


double ConvertUtils::convertCssLengthToPt(const std::wstring& cssValue) {
    // Aici apelezi o logica simplificata sau apelezi
    // functia cu 2 argumente, dar trimiți 0 sau o valoare implicită la al doilea argument.

    // CEA MAI BUNĂ PRACTICĂ: Apeși funcția cu 2 argumente,
    // pentru a evita duplicarea codului de parsare (DRY principle):

    // Dacă funcția cu 2 argumente este proiectată să returneze 0 pentru 'em' dacă fontSize e 0,
    // această metodă este cea mai curată.

    // Să presupunem că font size-ul implicit este 1.0 (sau 12.0)
    // Vom folosi un fontSize de 0.0 (sau o valoare dummy)
    return convertCssLengthToPt(cssValue, 1.0); // 1.0 e o valoare sigura pentru factor.
}

// ConvertUtils.cpp

double ConvertUtils::convertCssFontLengthToPt(const std::wstring& cssValue, double parentFontSize) {

    //return 12.0;
    if (cssValue.empty() || cssValue == L"normal") {
        // 'normal' este adesea folosit pentru line-height, echivalent cu 1.2 * font-size
        // sau pentru font-weight: normal. Dar aici returnam dimensiunea fontului parinte ca baza.
        return parentFontSize;
    }

    // Curățare (Trim)
    std::wstring trimmed = cssValue;
    trimmed.erase(0, trimmed.find_first_not_of(L" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(L" \t\n\r") + 1);

    if (trimmed.empty()) {
        return parentFontSize;
    }

    // 1. Parsare Valoare și Unitate
    try {
        size_t unit_pos = trimmed.find_first_not_of(L"0123456789.-");

        std::wstring val_str;
        std::wstring unit;
        double value;

        if (unit_pos == std::wstring::npos) {
            // Cazul A: Doar număr (ex: "1.5" pentru line-height)
            value = std::stod(trimmed);
            // Dacă e factor, returnăm factor * font-size-ul părintelui (sau curent, depinde de proprietate)
            return value * parentFontSize;
        }

        // Cazul B: Număr + unitate
        val_str = trimmed.substr(0, unit_pos);
        unit = trimmed.substr(unit_pos);
        std::transform(unit.begin(), unit.end(), unit.begin(), ::towlower);

        if (val_str.empty()) return parentFontSize;
        value = std::stod(val_str);

        // 2. Conversia în Puncte (Pt)

        // --- Unități relative (depind de parentFontSize) ---
        if (unit == L"em") {
            // 1em = parentFontSize
            return value * parentFontSize;
        }
        if (unit == L"%") {
            // 100% = parentFontSize
            return (value / 100.0) * parentFontSize;
        }

        // --- Unități absolute (le rezolvăm aici direct) ---
        if (unit == L"pt") {
            return value;
        }
        if (unit == L"px") {
            // Standard 96 DPI: 1px = 0.75pt
            return value * 0.75;
        }
        if (unit == L"mm") {
            // 1mm = 72 / 25.4 pt (~2.835 pt)
            return value * (72.0 / 25.4);
        }
        if (unit == L"in") {
            // 1in = 72pt
            return value * 72.0;
        }

        // Dacă nu se potrivește nicio unitate cunoscută
        return parentFontSize;
    }
    catch (...) {
        // Capturare eroare de parsare
        return parentFontSize;
    }
}



std::vector<std::wstring> ConvertUtils::tokenizeText(const std::wstring& text) {
    std::vector<std::wstring> tokens;
    std::wstring current;

    for (wchar_t ch : text) {
        if (ch == L' ' || ch == L'\t') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            tokens.push_back(std::wstring(1, ch)); // păstrează spațiul ca token separat
        }
        else if (ch == L'\n' || ch == L'\r') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            // opțional: păstrează newline ca token
            tokens.push_back(L"\n");
        }
        else {
            current += ch;
        }
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}